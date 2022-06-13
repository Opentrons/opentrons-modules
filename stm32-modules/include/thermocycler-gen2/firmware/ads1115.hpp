/**
 * @file ads1115.hpp
 * @brief
 * Driver file for the ADS1115 ADC. See ads1115.cpp for more details.
 */

#pragma once

#include <cstdint>
#include <variant>

#include "firmware/thermal_hardware.h"

namespace ADS1115 {

enum class Error {
    ADCTimeout, /**< Timed out waiting for ADC.*/
    I2CTimeout, /**< Timed out waiting for I2C.*/
    DoubleArm, /**< ADC already armed.*/
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
