#ifndef VENT_HARDWARE_H__
#define VENT_HARDWARE_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

void vent_hardware_init(void);
void hw_set_vent_state(VentState state);
VentState hw_get_vent_state(void);
bool hw_vent_fault_detected();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _VENT_HARDWARE_H__
