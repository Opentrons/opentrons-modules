#ifndef THERMISTOR_HARDWARE_H_
#define THERMISTOR_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void thermistor_hardware_init();

/**
 * @brief Writes a 16-bit value to the thermal subsystem I2C bus
 * @note Thread safe
 * @warning This function should \e only be called from a FreeRTOS
 * task context! It relies on a mutex to lock the commmunication
 * @param[in] addr The device address to write to
 * @param[in] reg The register address to write to
 * @param[in] val The two-byte value to write. The MSB is the first
 * byte written, and the LSB is the latter byte written.
 * @return true on success, false  on error
 */
bool thermal_i2c_write_16(uint16_t addr, uint8_t reg, uint16_t val);
/**
 * @brief Reads a 16-bit value from the thermal subsystem I2C bus
 * @note Thread safe
 * @warning This function should \e only be called from a FreeRTOS
 * task context! It relies on a mutex to lock the commmunication!
 * @param[in] addr The device address to read from
 * @param[in] reg The register address to read from
 * @param[out] val Returns the two-byte value that was read. The MSB is
 * the first byte read, and the LSB is the latter byte read.
 * @return true on success, false  on error
 */
bool thermal_i2c_read_16(uint16_t addr, uint8_t reg, uint16_t *val);

/**
 * @brief Configure the current task to become unblocked on the next call
 * to \ref thermal_adc_ready_callback
 */
bool thermal_arm_adc_for_read();

/**
 * @brief Callback when an ADC READY pin interrupt is triggered (falling edge)
 */
void thermal_adc_ready_callback();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMISTOR_HARDWARE_H_ */
