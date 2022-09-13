#ifndef THERMISTOR_HARDWARE_H_
#define THERMISTOR_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void thermistor_hardware_init();

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
