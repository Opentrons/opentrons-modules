/**
 * @file ws2812.hpp
 * @brief Provides a class for controlling WS2812 integrated light sources,
 * also known as Neopixels.
 * @details The WS2812 structure encapsulates the configuration of a single
 * pixel on a string of WS2812's, but the 24-bit value cannot just be sent
 * directly to the WS2812. The WS2812 follows a single-wire protocol at
 * 800kHz (1.25 uS) using PWM to control each bit. A 1 is represented by a
 * PWM of  56%, while a 0 is represented by a PWM of 28%.
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
 * is disabled to mark to the WS2812 chain that the message is completed.
 */
#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>

namespace ws2812 {

/**
 * @brief Policy for writing to the WS2812 over DMA
 * @tparam Policy Class that provides the types
 * @tparam Buffer Should be an std::array of an unsigned
 * integer type
 */
template <typename Policy, typename BufferT>
concept WS2812Policy = requires(Policy& p, BufferT b) {
    // Function to start sending a buffer. Accepts a buffer b
    { p.start_send(b) } -> std::same_as<bool>;
    // Function to end the data transmission. Cannot fail.
    {p.end_send()};
    // Function to delay for a DMA interrupt. Accepts a timeout
    // in milliseconds.
    { p.wait_for_interrupt(100) } -> std::same_as<bool>;
    // Function to get the max PWM configured for the timer. Should
    // be in the same byte format as the PWM buffer type passed in.
    { p.get_max_pwm() } -> std::same_as<typename BufferT::value_type>;
    std::unsigned_integral<typename BufferT::value_type>;
};

// This class represents a single WS2812
struct WS2812 {
    // Data is sent in the order G, R, B
    uint8_t g = 0, r = 0, b = 0;
};

/**
 * @brief Class encapsulating a string of connected WS2812 elements.
 *
 * @tparam PWM The type of the register that controls PWM width. Must
 * be an integral type
 * @tparam N The number of WS2812 elements in the string
 */
template <typename PWM, size_t N>
requires std::unsigned_integral<PWM>
class WS2812String {
  public:
    // 8 bits per color, 3 colors
    static constexpr size_t SINGLE_PIXEL_BUF_SIZE = 8 * 3;
    // Two pixels of data
    static constexpr size_t DOUBLE_PIXEL_BUF_SIZE = SINGLE_PIXEL_BUF_SIZE * 2;
    // To send a logical 1, set PWM to this value
    static constexpr double PWM_ON_PERCENTAGE = 0.56;
    // To send a logical 0, set PWM to this value
    static constexpr double PWM_OFF_PERCENTAGE = 0.28;
    // To send a logical STOP, set PWM to this value
    static constexpr PWM PWM_STOP_VALUE = 0;
    // Time to wait for an interrupt in milliseconds
    static constexpr uint32_t INTERRUPT_DELAY_MAX = 100;
    // Type of the array of pixels
    using PixelBuffer = std::array<WS2812, N>;
    // Type of the output buffer (raw register values for DMA)
    using OutputBuffer = std::array<PWM, DOUBLE_PIXEL_BUF_SIZE>;
    // Type of an iterator on the output buffer
    using OutputBufferItr = OutputBuffer::iterator;

    WS2812String() : _pixels(), _pwm_buffer(), _max_pwm(0) {}

    /**
     * @brief Write the current pixel buffer to a string of WS2812
     *
     * @tparam Policy Hardware abstraction layer to send PWM data
     * @param policy Instance of the hardware abstraction policy
     * @return True if the data could be sent, false otherwise.
     */
    template <typename Policy>
    requires WS2812Policy<Policy, OutputBuffer>
    auto write(Policy& policy) -> bool {
        // Get the max PWM value (whatever represents 100%)
        _max_pwm = policy.get_max_pwm();
        for (auto& pwm : _pwm_buffer) {
            pwm = PWM_STOP_VALUE;
        }
        // First write the first two pixels
        auto itr = _pwm_buffer.begin();
        if (N > 0) {
            auto ret = serialize_pixel(_pixels.at(0), itr);
            if (!ret.has_value()) {
                return false;
            }
            if (N > 1) {
                ret = serialize_pixel(_pixels.at(1), ret.value());
                if (!ret.has_value()) {
                    return false;
                }
            }
            itr = _pwm_buffer.begin();
        }
        if (!policy.start_send(_pwm_buffer)) {
            return false;
        }
        // For each pixel, we get one interrupt completion.
        // After the interrupt, overwrite the data in the part of the buffer
        // that was just sent.
        for (size_t i = 1; i < N; ++i) {
            if (!policy.wait_for_interrupt(INTERRUPT_DELAY_MAX)) {
                policy.end_send();
                return false;
            }
            if (i < N - 1) {
                auto ret = serialize_pixel(_pixels.at(i + 1), itr);
                if (!ret.has_value()) {
                    policy.end_send();
                    return false;
                }
                itr = ret.value();
                if (itr == _pwm_buffer.end()) {
                    itr = _pwm_buffer.begin();
                }
            }
        }
        // Once we send the last pixel, stop sending new ones.
        // The protocl on the LEDs is that the first packet is for the
        // first pixel, the second packet for the second pixel, etc. So
        // if stopping is too slow and some extra bytes get sent, it is
        // okay.
        if (!policy.wait_for_interrupt(INTERRUPT_DELAY_MAX)) {
            policy.end_send();
            return false;
        }
        policy.end_send();
        return true;
    }

    /**
     * @brief Obtain a reference to a pixel at index \c i
     *
     * @param i The index to get a pixel from
     * @return WS2812& Reference to the pixel
     */
    [[nodiscard]] auto pixel(size_t i) -> WS2812& { return _pixels.at(i); }

    /**
     * @brief Set all of the pixels to the same color value
     *
     * @param val Color value to set all pixels to
     */
    auto set_all(WS2812 val) -> void {
        for (auto& pixel : _pixels) {
            pixel = val;
        }
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
    auto serialize_pixel(WS2812& pixel, OutputBufferItr itr)
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
        if (itr == _pwm_buffer.end()) {
            return RT();
        }
        for (int i = 0; i < 8; ++i, byte = byte << 1) {
            if (itr == _pwm_buffer.end()) {
                return RT();
            }
            auto percentage = ((byte & 0x80) == 0x80) ? PWM_ON_PERCENTAGE
                                                      : PWM_OFF_PERCENTAGE;
            *itr = static_cast<PWM>(percentage * static_cast<double>(_max_pwm));
            std::advance(itr, 1);
        }
        return RT(itr);
    }

    PixelBuffer _pixels;
    OutputBuffer _pwm_buffer;
    PWM _max_pwm;
};

}  // namespace ws2812
