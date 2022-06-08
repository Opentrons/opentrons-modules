/**
 * @file ads1115.hpp
 * @brief
 * Driver file for the ADS1115 ADC. See ads1115.cpp for more details.
 */

#pragma once

#include <cstdint>
#include <variant>
#include <optional>

#include "firmware/thermal_hardware.h"

namespace ADS1115 {

/**
 * @brief The ADS1115 Policy is structured to allow multiple instances of the
 * ADS1115::ADC class to reference the same exact 
 * 
 * @tparam Policy 
 */
template<typename Policy>
concept ADS1115Policy = requires(Policy& p, size_t id, uint8_t u8, uint16_t u16) {
    // A constant to get the number of ADS1115 on the system, and in effect
    // get the max ID of an ADS1115
    std::same_as<size_t, std::decay_t<decltype(Policy::ADS1115_COUNT)>>;
    // A function to mark that an ADS1115 was initialized. If this returns
    // false, that means that the ADS1115 initialization was already started
    // or already completed.
    {p.ads1115_mark_initialized(id)} -> std::same_as<bool>;
    // A function to check that an ADS1115 was initialized
    {p.ads1115_check_initialized(id)} -> std::same_as<bool>;
    // Acquire the mutex for this ADC
    {p.ads1115_get_lock(id)} -> {std::same_as<bool>};
    // Release the mutex for this ADC
    {p.ads1115_release_lock(id)} -> {std::same_as<bool>};
    // Arm this ADC for a read operation
    {p.ads1115_arm_for_read(id)} -> {std::same_as<bool>};
    // Write a 16 bit register
    {p.ads1115_i2c_write_16(u8, u8, u16)} -> {std::same_as<bool>};
    // Read a 16 bit register
    {p.ads1115_i2c_read_16(u8, u8)} -> {std::same_as<std::optional<uint16_t>};
};

enum class Error {
    ADCTimeout, /**< Timed out waiting for ADC.*/
    ADCPin,     /**< Pin is not allowed.*/
    ADCInit     /**< ADC is not initialized.*/
};

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
    ADC(uint8_t addr, ADC_ITR_T id);

    /**
     * @brief Initialize the ADC. If run multiple times on the same ADC,
     * this returns success. Will assert on inability to communicate with
     * the ADC.
     * @note Thread safe
     * @warning Only call this from a FreeRTOS thread context.
     */
    void initialize();
    /**
     * @brief Read a value from the ADC.
     * @note Thread safe
     * @warning Only call this from a FreeRTOS thread context.
     * @param[in] pin The pin to read. Must be a value in the range
     * [0, \ref pin_count)
     * @return The value read by the ADC in ADC counts, or an error.
     */
    auto read(uint16_t pin) -> ReadVal;

    /**
     * @brief Check if this ADC is initialized
     * @return True if initialized, false if not.
     */
    auto initialized() -> bool;

  private:
    uint8_t _addr;
    ADC_ITR_T _id;
    uint16_t _last_result;

    auto get_lock() -> bool;
    auto release_lock() -> bool;

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
    /** Maximum time to wait for the semaphor, in milliseconds.*/
    static constexpr int max_semaphor_wait = 400;
};

}  // namespace ADS1115
