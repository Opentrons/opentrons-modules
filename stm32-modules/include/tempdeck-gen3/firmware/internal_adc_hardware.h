/**
 * @file internal_adc_hardware.h
 * @brief This file provides an interface to the internal ADC on the 
 * Tempdeck MCU.
 * 
 * The internal ADC is used to monitor three feedback signals from the
 * peltier: a current sense feedback input, and two independent voltage
 * level monitors (one per peltier channel).
 * 
 * The internal ADC is configured to run in a fully interrupt-based mode.
 * The ADC will read the current value of each of the feedback channels, and
 * once all of the conversions are done it will write those values back to
 * memory.
 * 
 */

#ifndef INTERNAL_ADC_HARDWARE_H_
#define INTERNAL_ADC_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void internal_adc_init();

bool internal_adc_start_reading();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif /* INTERNAL_ADC_HARDWARE_H_ */
