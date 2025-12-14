#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void pressure_sensor_hardware_init(void);
bool sensor_hardware_read_eoc_pin(PressureSensorID sensor_id);
void sensor_hardware_sensor_reset(PressureSensorID sensor_id);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
