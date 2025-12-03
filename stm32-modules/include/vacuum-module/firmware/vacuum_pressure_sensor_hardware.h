#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void vacuum_pressure_sensor_hardware_init(void);
bool sensor_hardware_read_eoc_pin(VacuumPressureSensorId sensor_id);
void sensor_hardware_sensor_reset(VacuumPressureSensorId sensor_id);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus