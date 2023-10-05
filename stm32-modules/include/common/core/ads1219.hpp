/**
 * @file ads1219.hpp
 * @brief Driver file for the ADS1219 ADC.
 */

#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <variant>

namespace ADS1219 {

/**
 * @brief The way that the ADS1219 policy is structured assumes that each
 * ADC instance is provided an instance of a Policy type that is aware of
 * which ADC it is talking to, so there is no need to specify the address
 * or any other enumeration of the ADC
 */
template <typename Policy>
concept ADS1219Policy = requires(Policy& p, uint8_t u8, uint32_t u32,
                                 std::array<uint8_t, 1> one_byte,
                                 std::array<uint8_t, 2> two_bytes,
                                 std::array<uint8_t, 3> &three_bytes) {
    // A function to mark that an ADS1219 was initialized.
    { p.ads1219_mark_initialized() } -> std::same_as<void>;
    // A function to check that an ADS1219 was initialized
    { p.ads1219_check_initialized() } -> std::same_as<bool>;
    // Acquire the mutex for this ADC. The mutex must be initialized with the
    // policy, so it is always valid
    { p.ads1219_get_lock() } -> std::same_as<void>;
    // Release the mutex for this ADC. The mutex must be initialized with the
    // policy, so it is always valid
    { p.ads1219_release_lock() } -> std::same_as<void>;
    // Arm this ADC for a read operation
    { p.ads1219_arm_for_read() } -> std::same_as<bool>;
    // Send an array of data - must work for both 1 and 2 byte messages
    { p.ads1219_i2c_send_data(one_byte) } -> std::same_as<bool>;
    { p.ads1219_i2c_send_data(two_bytes) } -> std::same_as<bool>;
    // Read a chunk of data
    { p.ads1219_i2c_read_data(three_bytes) } -> std::same_as<bool>;
    // Waits for a pulse from the ADC that was armed by this task. Maximum
    // wait time is passed as a parameter.
    { p.ads1219_wait_for_pulse(u32) } -> std::same_as<bool>;
};

enum class Error {
    ADCTimeout = 1, /**< Timed out waiting for ADC.*/
    I2CTimeout = 2, /**< Timed out waiting for I2C.*/
    DoubleArm = 3,  /**< ADC already armed.*/
    ADCPin = 4,     /**< Pin is not allowed.*/
    ADCInit = 5     /**< ADC is not initialized.*/
};

template <ADS1219Policy Policy>
class ADC {
  public:
    using ReadVal = std::variant<uint32_t, Error>;

    ADC() = delete;
    /**
     * @brief Construct a new ADS1219 ADC
     * @param[in] addr The I2C address of the ADC
     * @param[in] id The ID of the ADC, which will link to one of the
     * interrupts defined in \ref thermal_hardware.h
     */
    explicit ADC(Policy& policy) : _policy(policy) {}

    /**
     * @brief Initialize the ADC. If run multiple times on the same ADC,
     * this returns success. Will assert on inability to communicate with
     * the ADC.
     * @note Thread safe
     * @warning Only call this from a FreeRTOS thread context.
     */
    void initialize() {
        // If this function returns true, we need to perform the
        // init routine for this ADC.
        get_lock();
        if (!_policy.ads1219_check_initialized()) {
            std::array<uint8_t, 1> data = {RESET_COMMAND};
            _policy.ads1219_i2c_send_data(data);

            _policy.ads1219_mark_initialized();
        }
        release_lock();
    }
    /**
     * @brief Read a value from the ADC.
     * @note Thread safe
     * @warning Only call this from a FreeRTOS thread context.
     * @param[in] pin The pin to read. Must be a value in the range
     * [0, \ref pin_count)
     * @return The value read by the ADC in ADC counts, or an error.
     */
    auto read(uint8_t pin) -> ReadVal {
        if (!initialized()) {
            return ReadVal(Error::ADCInit);
        }
        if (!(pin < pin_count)) {
            return ReadVal(Error::ADCPin);
        }
        get_lock();

        auto ret = _policy.ads1219_arm_for_read();
        if (!ret) {
            release_lock();
            return ReadVal(Error::DoubleArm);
        }

        // Configure the input pin
        std::array<uint8_t, 2> config_data = {
            WREG_CONFIG_COMMAND, pin_to_config_reg(pin)
        };
        ret = _policy.ads1219_i2c_send_data(config_data);
        if (!ret) {
            release_lock();
            return ReadVal(Error::I2CTimeout);
        }

        // Start the new reading
        ret = send_command(START_COMMAND);
        if (!ret) {
            release_lock();
            return ReadVal(Error::I2CTimeout);
        }

        ret = _policy.ads1219_wait_for_pulse(max_pulse_wait_ms);
        if (!ret) {
            release_lock();
            return ReadVal(Error::ADCTimeout);
        }

        // Send command so that next read will give the conversion data
        ret = send_command(RDATA_COMMAND);
        if (!ret) {
            release_lock();
            return ReadVal(Error::I2CTimeout);
        }

        // Read the result
        std::array<uint8_t, 3> result{};
        ret = _policy.ads1219_i2c_read_data(result);

        release_lock();
        if (ret) {
            uint32_t value = 
                (static_cast<uint32_t>(result[0]) << 16) |
                (static_cast<uint32_t>(result[1]) << 8) |
                (static_cast<uint32_t>(result[2]));
            return ReadVal(value);
        }
        return ReadVal(Error::I2CTimeout);
    }

    /**
     * @brief Check if this ADC is initialized
     * @return True if initialized, false if not.
     */
    auto initialized() -> bool { return _policy.ads1219_check_initialized(); }

  private:
    Policy& _policy;

    auto inline get_lock() -> void { _policy.ads1219_get_lock(); }
    auto inline release_lock() -> void { _policy.ads1219_release_lock(); }

    /**
     * @brief Given a pin input, return the value that should be OR'd to
     * the configuration register to set the input pin mode correctly.
     */
    static inline auto pin_to_config_reg(uint8_t pin) -> uint8_t {
        // The pin mux value for reading Ch0. All other channels increment from here.
        static constexpr uint8_t PIN_MUX_CH0 = 3;
        // The amount to left-shift the pin mux mask
        static constexpr uint8_t PIN_MUX_SHIFT = 5;
        return static_cast<uint8_t>((PIN_MUX_CH0 + pin) << PIN_MUX_SHIFT) | CONFIG_REG_DEFAULT;
    }

    /**
     * @brief Send a single byte command to the ADS1219
     */
    auto inline send_command(uint8_t cmd) -> bool {
        std::array<uint8_t, 1> data = {cmd};
        return _policy.ads1219_i2c_send_data(data);
    }

    /** Send this byte to reset the IC.*/
    static constexpr uint8_t RESET_COMMAND = 0x06;

    /** Default settings include:
     *   - Data rate of 20Hz
     *   - Gain of 1x
     *   - Single-shot mode
     *   - Internal VRef
     */
    static constexpr uint8_t CONFIG_REG_DEFAULT = 0x00;
    
    /** Send this byte to start a new reading.*/
    static constexpr uint8_t START_COMMAND = 0x08;
    /** Send this command to read the conversion results.*/
    static constexpr uint8_t RDATA_COMMAND = 0x10;
    /** Send this command to write the configuration register.*/
    static constexpr uint8_t WREG_CONFIG_COMMAND = 0x40;
    
    /** Number of pins on the ADC.*/
    static constexpr uint16_t pin_count = 4;
    /** Maximum time to wait for the pulse, in milliseconds.*/
    static constexpr int max_pulse_wait_ms = 500;
};

}  // namespace ADS1219
