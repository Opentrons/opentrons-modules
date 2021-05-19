#ifndef __HEATER_HARDWARE_H_
#define __HEATER_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>

#include "stm32f3xx_hal.h"

typedef enum {
    NTC_PAD_A = 1,
    NTC_PAD_B = 2,
    NTC_ONBOARD = 3,
} ntc_selection;

typedef struct {
    uint16_t pad_a_val;
    uint16_t pad_b_val;
    uint16_t onboard_val;
} conversion_results;

typedef struct {
    ADC_HandleTypeDef ntc_adc;
    void (*conversions_complete)(const conversion_results* results);
    void* hardware_internal;
} heater_hardware;

void heater_hardware_setup(heater_hardware* hardware);
void heater_hardware_teardown(heater_hardware* hardware);
void heater_hardware_begin_conversions(heater_hardware* hardware);
bool heater_hardware_sense_power_good();
void heater_hardware_drive_pg_latch_low();
void heater_hardware_release_pg_latch();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __HEATER_HARDWARE_H_
