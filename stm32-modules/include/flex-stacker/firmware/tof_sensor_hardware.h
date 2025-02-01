#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void tof_hardware_init(void);
void enable_eeprom_write(bool enable);
void enable_tof_sensor_write(TOFSensorID sensor_id, bool enable);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
