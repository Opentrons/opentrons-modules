#ifndef THERMAL_HARDWARE_H__
#define THERMAL_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "firmware/thermal_peltier_hardware.h"

/** Defines the interrupt pins available for reading results.*/
typedef enum { ADC1_ITR, ADC2_ITR } ADC_ITR_T;

/** Cloned interrupt to use names for the ADC's.*/
typedef enum { ADC_FRONT = ADC1_ITR, ADC_REAR = ADC2_ITR } ADC_ID_t;

#define ADC_ITR_NUM (2)

/**
 * Initializes all thermal hardware. Sets a static, thread-safe
 * variable to indicate completion to thermal_hardware_wait_for_init
 */
void thermal_hardware_setup(void);

/**
 * Waits until thermal hardware is initialied.
 * @warning Only call this from a FreeRTOS thread context.
 */
void thermal_hardware_wait_for_init(void);

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
 * @brief Configures one of the ADC Alert monitoring pins to be ready
 * to signal the correct task after a conversion-complete signals is raised.
 * @param[in] id The ADC to arm the callback for.
 */
bool thermal_arm_adc_for_read(ADC_ITR_T id);

/**
 * @brief Callback when an ADC READY pin interrupt is triggered (falling edge)
 */
void thermal_adc_ready_callback(ADC_ITR_T id);

/**
 * @brief This function handles I2C2 event interrupt / I2C2 wake-up interrupt
 * through EXTI line 24.
 */
void I2C2_EV_IRQHandler(void);
/**
 * @brief This function handles I2C2 error interrupt.
 */
void I2C2_ER_IRQHandler(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMAL_HARDWARE_H__ */
