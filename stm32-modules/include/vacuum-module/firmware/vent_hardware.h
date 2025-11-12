#ifndef VENT_HARDWARE_H__
#define VENT_HARDWARE_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

void vent_hardware_init(void);
void hw_open_vent(bool open);
bool hw_vent_fault_detected();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _VENT_HARDWARE_H__
