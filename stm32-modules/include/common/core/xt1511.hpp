/**
 * @file xt1511.hpp
 * @brief Provides a class for controlling XT1511 integrated light sources,
 * also known as Neopixels.
 * @details The XT1511 structure encapsulates the configuration of a single
 * pixel on a string of XT1511's, but the 32-bit value cannot just be sent
 * directly to the XT1511. The XT1511 follows a single-wire protocol at
 * 800kHz (1.25 uS) using PWM to control each bit. A 1 is represented by a
 * PWM of  56%, while a 0 is represented by a PWM of 28%.
 *
 * The XT1511 also supports a 400kHz protocol (2.5 uS) wherein the PWM values
 * become 20% and 48% for 0 and 1, respectively.
 *
 * Storing the full buffer of pixel data would use a large amount of RAM.
 * Each pixel requires 24 PWM values, and with a 16-bit PWM register the
 * buffer would have to contain multiple kilobytes of data for a 16-pixel
 * chain. Instead, a double buffering method is used. When starting a write,
 * a buffer with the first two pixels of data is passed to a function which
 * starts a circular DMA operation. Each time the Half or Full Complete
 * interrupt is triggered, either the first or second half (respectively)
 * of the buffer is loaded with the next pixel of data. This repeats until
 * the entire chain of pixels has been sent, at which point the PWM output
 * is disabled to mark to the XT1511 chain that the message is completed.
 */
#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>

namespace xt1511 {

// 8 bits per color, 4 colors
static constexpr size_t SINGLE_PIXEL_BUF_SIZE = 32;

/**
 * @brief Policy for writing to the XT1511 over DMA
 * @tparam Policy Class that provides the types
 * @tparam Buffer Should be an std::array of an unsigned
 * integer type
 */
template <typename Policy, typename BufferT>
concept XT1511Policy = requires(Policy& p, BufferT& b) {
    // Function to start sending a buffer. Accepts a buffer b
    { p.start_send(b) } -> std::same_as<bool>;
    // Function to end the data transmission. Cannot fail.
    {p.end_send()};
    // Function to delay for a DMA interrupt. Accepts a timeout
    // in milliseconds.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    { p.wait_for_interrupt(100) } -> std::same_as<bool>;
    // Function to get the max PWM configured for the timer. Should
    // be in the same byte format as the PWM buffer type passed in.
    { p.get_max_pwm() } -> std::same_as<typename BufferT::value_type>;
    std::unsigned_integral<typename BufferT::value_type>;
};

// This class represents a single XT1511
struct XT1511 {
    // Data is sent in the order G, R, B, W
    uint8_t g = 0, r = 0, b = 0, w = 0;
    auto set_scale(double scale) {
        g *= scale;
        r *= scale;
        b *= scale;
        w *= scale;
    }
};

auto operator==(const XT1511& l, const XT1511& r) -> bool;

enum class Speed {
    FULL, /**< 800kHz.*/
    HALF, /**< 400kHz.*/
};

/**
 * @brief Class encapsulating a string of connected XT1511 elements.
 *
 * @tparam PWM The type of the register that controls PWM width. Must
 * be an integral type
 * @tparam N The number of XT1511 elements in the string
 */
template <typename PWM, size_t N>
requires std::unsigned_integral<PWM>
class XT1511String {
  public:
    // To send a logical 1, set PWM to this value
    static constexpr double PWM_ON_FULL_SPEED = 0.56;
    // To send a logical 0, set PWM to this value
    static constexpr double PWM_OFF_FULL_SPEED = 0.28;
    // To send a logical 1, set PWM to this value
    static constexpr double PWM_ON_HALF_SPEED = 0.48;
    // To send a logical 0, set PWM to this value
    static constexpr double PWM_OFF_HALF_SPEED = 0.20;
    // To send a logical STOP, set PWM to this value
    static constexpr PWM PWM_STOP_VALUE = 0;
    // Time to wait for an interrupt in milliseconds
    static constexpr uint32_t INTERRUPT_DELAY_MAX = 100;
    // Type of the array of pixels
    using PixelBuffer = std::array<XT1511, N>;
    // Type of the output buffer (raw register values for DMA)
    // Includes enough space for every pixel + a 0 to turn off PWM
    using OutputBuffer = std::array<PWM, (SINGLE_PIXEL_BUF_SIZE * N) + 1>;
    // Type of an iterator on the output buffer
    using OutputBufferItr = typename OutputBuffer::iterator;

    XT1511String(Speed speed = Speed::FULL)
        : _pixels(), _pwm_buffer(), _max_pwm(0), _speed(speed) {}

    /**
     * @brief Write the current pixel buffer to a string of XT1511
     *
     * @tparam Policy Hardware abstraction layer to send PWM data
     * @param policy Instance of the hardware abstraction policy
     * @return True if the data could be sent, false otherwise.
     */
    template <typename Policy>
    requires XT1511Policy<Policy, OutputBuffer>
    auto write(Policy& policy) -> bool {
        // Get the max PWM value (whatever represents 100%)
        _max_pwm = policy.get_max_pwm();
        auto* itr = _pwm_buffer.begin();
        for (auto& pixel : _pixels) {
            auto ret = serialize_pixel(pixel, itr);
            if (!ret.has_value()) {
                return false;
            }
            itr = ret.value();
        }
        *itr = PWM_STOP_VALUE;
        if (!policy.start_send(_pwm_buffer)) {
            return false;
        }
        auto ret = policy.wait_for_interrupt(INTERRUPT_DELAY_MAX);
        policy.end_send();
        return ret;
    }

    /**
     * @brief Obtain a reference to a pixel at index \c i
     *
     * @param i The index to get a pixel from
     * @return XT1511& Reference to the pixel
     */
    [[nodiscard]] auto pixel(size_t i) -> XT1511& { return _pixels.at(i); }

    /**
     * @brief Set all of the pixels to the same color value
     *
     * @param val Color value to set all pixels to
     */
    auto set_all(XT1511 val) -> void {
        for (auto& pixel : _pixels) {
            pixel = val;
        }
    }

    [[nodiscard]] auto inline pwm_on_percentage() const -> double {
        if (_speed == Speed::FULL) {
            return PWM_ON_FULL_SPEED;
        }
        return PWM_ON_HALF_SPEED;
    }

    [[nodiscard]] auto inline pwm_off_percentage() const -> double {
        if (_speed == Speed::FULL) {
            return PWM_OFF_FULL_SPEED;
        }
        return PWM_OFF_HALF_SPEED;
    }

  private:
    /**
     * @brief Serialize one pixel into the pwm buffer
     *
     * @param[in] pixel The pixel to serialize
     * @param[in] itr Iterator to the buffer to start writing at
     * @return OutputBufferItr pointing to one after the last location
     * written to, or an empty return if the end is reached prematurely
     */
    auto serialize_pixel(const XT1511& pixel, OutputBufferItr itr)
        -> std::optional<OutputBufferItr> {
        using RT = std::optional<OutputBufferItr>;
        if (itr == _pwm_buffer.end()) {
            return RT();
        }
        auto ret = serialize_byte(pixel.g, itr);
        if (!ret.has_value()) {
            return RT();
        }
        ret = serialize_byte(pixel.r, ret.value());
        if (!ret.has_value()) {
            return RT();
        }
        ret = serialize_byte(pixel.b, ret.value());
        if (!ret.has_value()) {
            return RT();
        }
        ret = serialize_byte(pixel.w, ret.value());
        return ret;
    }

    /**
     * @brief Serialize one byte of pixel information
     *
     * @param[in] byte The byte to serialize
     * @param[in] itr Iterator to the buffer to start writing at
     * @return OutputBufferItr pointing to one after the last location
     * written to, or an empty return if the end is reached prematurely
     */
    auto serialize_byte(uint8_t byte, OutputBufferItr itr)
        -> std::optional<OutputBufferItr> {
        using RT = std::optional<OutputBufferItr>;
        constexpr uint8_t msb_mask = 0x80;
        if (itr == _pwm_buffer.end()) {
            return RT();
        }
        for (int i = 0; i < 8; ++i, byte = byte << 1) {
            if (itr == _pwm_buffer.end()) {
                return RT();
            }
            auto percentage = ((byte & msb_mask) == msb_mask)
                                  ? pwm_on_percentage()
                                  : pwm_off_percentage();
            *itr = static_cast<PWM>(percentage * static_cast<double>(_max_pwm));
            std::advance(itr, 1);
        }
        return RT(itr);
    }

    PixelBuffer _pixels;
    OutputBuffer _pwm_buffer;
    PWM _max_pwm;
    const Speed _speed;
};

}  // namespace xt1511
