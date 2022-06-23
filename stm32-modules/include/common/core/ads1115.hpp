/**
 * @file ads1115.hpp
 * @brief
 * Driver file for the ADS1115 ADC. See ads1115.cpp for more details.
 */

#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <variant>

namespace ADS1115 {

/**
 * @brief The way that the ADS1115 policy is structured assumes that each
 * ADC instance is provided an instance of a Policy type that is aware of
 * which ADC it is talking to, so there is no need to specify the address
 * or any other enumeration of the ADC
 */
template <typename Policy>
concept ADS1115Policy = requires(Policy& p, uint8_t u8, uint16_t u16,
                                 uint32_t u32) {
    // A function to mark that an ADS1115 was initialized.
    { p.ads1115_mark_initialized() } -> std::same_as<void>;
    // A function to check that an ADS1115 was initialized
    { p.ads1115_check_initialized() } -> std::same_as<bool>;
    // Acquire the mutex for this ADC. The mutex must be initialized with the
    // policy, so it is always valid
    { p.ads1115_get_lock() } -> std::same_as<void>;
    // Release the mutex for this ADC. The mutex must be initialized with the
    // policy, so it is always valid
    { p.ads1115_release_lock() } -> std::same_as<void>;
    // Arm this ADC for a read operation
    { p.ads1115_arm_for_read() } -> std::same_as<bool>;
    // Write a 16 bit register
    { p.ads1115_i2c_write_16(u8, u16) } -> std::same_as<bool>;
    // Read a 16 bit register
    { p.ads1115_i2c_read_16(u8) } -> std::same_as<std::optional<uint16_t>>;
    // Waits for a pulse from the ADC that was armed by this task. Maximum
    // wait time is passed as a parameter.
    { p.ads1115_wait_for_pulse(u32) } -> std::same_as<bool>;
};

enum class Error {
    ADCTimeout = 1, /**< Timed out waiting for ADC.*/
    I2CTimeout = 2, /**< Timed out waiting for I2C.*/
    DoubleArm = 3,  /**< ADC already armed.*/
    ADCPin = 4,     /**< Pin is not allowed.*/
    ADCInit = 5     /**< ADC is not initialized.*/
};

template <ADS1115Policy Policy>
class ADC {
  public:
    using ReadVal = std::variant<uint16_t, Error>;

    ADC() = delete;
    /**
     * @brief Construct a new ADS1115 ADC
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
        if (!_policy.ads1115_check_initialized()) {
            reg_write(lo_thresh_addr, lo_thresh_default);
            reg_write(hi_thresh_addr, hi_thresh_default);
            reg_write(config_addr, config_default);

            _policy.ads1115_mark_initialized();
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
    auto read(uint16_t pin) -> ReadVal {
        if (!initialized()) {
            return ReadVal(Error::ADCInit);
        }
        if (!(pin < pin_count)) {
            return ReadVal(Error::ADCPin);
        }
        get_lock();

        auto ret = _policy.ads1115_arm_for_read();
        if (!ret) {
            release_lock();
            return ReadVal(Error::DoubleArm);
        }
        ret =
            reg_write(config_addr, config_default | (pin << config_mux_shift) |
                                       config_start_read);
        if (!ret) {
            release_lock();
            return ReadVal(Error::I2CTimeout);
        }

        ret = _policy.ads1115_wait_for_pulse(max_pulse_wait_ms);
        if (!ret) {
            release_lock();
            return ReadVal(Error::ADCTimeout);
        }

        auto result = reg_read(conversion_addr);
        release_lock();
        if (result.has_value()) {
            return ReadVal(result.value());
        }
        return ReadVal(Error::I2CTimeout);
    }

    /**
     * @brief Check if this ADC is initialized
     * @return True if initialized, false if not.
     */
    auto initialized() -> bool { return _policy.ads1115_check_initialized(); }

  private:
    Policy& _policy;

    auto inline get_lock() -> void { _policy.ads1115_get_lock(); }
    auto inline release_lock() -> void { _policy.ads1115_release_lock(); }

    auto inline reg_write(uint8_t reg, uint16_t data) -> bool {
        return _policy.ads1115_i2c_write_16(reg, data);
    }

    auto inline reg_read(uint8_t reg) -> std::optional<uint16_t> {
        return _policy.ads1115_i2c_read_16(reg);
    }

    static constexpr uint8_t conversion_addr = 0x00;
    static constexpr uint8_t config_addr = 0x01;
    static constexpr uint8_t lo_thresh_addr = 0x02;
    static constexpr uint8_t hi_thresh_addr = 0x03;
    /** Need to write this to enable RDY pin.*/
    static constexpr uint16_t lo_thresh_default = 0x0000;
    /** Need to write this to enable RDY pin.*/
    static constexpr uint16_t hi_thresh_default = 0x8000;
    /** Not the startup default, but the value to write on startup.
     * - Input will be from AINx to GND instead of differential
     * - Gain amplifier is set to +/- 2.048 V
     * - Single shot mode
     * - Data rate is 250 SPS
     * - Default comparator values, except for enabling the ALERT/RDY pin
     */
    static constexpr uint16_t config_default = 0x45A0;
    /** Set this bit to start a read.*/
    static constexpr uint16_t config_start_read = 0x8000;
    /** Shift the pin setting by this many bits to set the input pin.*/
    static constexpr uint16_t config_mux_shift = 12;
    /** Number of pins on the ADC.*/
    static constexpr uint16_t pin_count = 4;
    /** Maximum time to wait for the pulse, in milliseconds.*/
    static constexpr int max_pulse_wait_ms = 500;
};

}  // namespace ADS1115
