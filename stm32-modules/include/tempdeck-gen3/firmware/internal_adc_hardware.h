/**
 * @file internal_adc_hardware.h
 * @brief This file provides an interface to the internal ADC on the 
 * Tempdeck MCU.
 * 
 * The internal ADC is used to monitor a current sense feedback input.
 * 
 * The internal ADC is configured to run in a fully interrupt-based mode.
 * The ADC will read a series of samples from the feedback channel, writing
 * the values back over DMA. Once the conversions are all complete, it will
 * invoke a configurable callback to tell higher level firmware that the 
 * readings array is populated and ready to use.
 * 
 */

#ifndef INTERNAL_ADC_HARDWARE_H_
#define INTERNAL_ADC_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#define GET_ADC_AVERAGE_ERR (0xFFFFFFFF)

#define INTERNAL_ADC_READING_COUNT (8)

/**
 * @brief Initialize the internal ADC hardware. This function is
 * thread safe and guaranteed to only initialze one time.
 * 
 */
void internal_adc_init();

/**
 * @brief Start a new series of readings from the ADC
 * 
 * @return true if the readings could be started, false otherwise
 */
bool internal_adc_start_readings();

/**
 * @brief Get the averaged ADC reading from the last batch of readings.
 * 
 * @return The average ADC value if it is available, or returns
 * \ref GET_ADC_AVERAGE_ERR if the readings are unavailable.
 */
uint32_t internal_adc_get_average();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif /* INTERNAL_ADC_HARDWARE_H_ */
