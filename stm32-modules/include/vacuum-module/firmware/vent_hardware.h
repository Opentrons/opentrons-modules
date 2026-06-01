#ifndef VENT_HARDWARE_H__
#define VENT_HARDWARE_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#define REF_VOLTAGE 3.3
#define DAC_FULLRANGE 256

void vent_hardware_init(void);
void hw_set_vent_state(VentState state);
void hw_set_vent_voltage(double volt);
VentState hw_get_vent_state(void);
bool hw_vent_fault_detected();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _VENT_HARDWARE_H__
