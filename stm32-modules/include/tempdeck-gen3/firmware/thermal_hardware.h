#ifndef THERMAL_HARDWARE_H_
#define THERMAL_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void thermal_hardware_init();

void thermal_hardware_enable_peltiers();
void thermal_hardware_disable_peltiers();

bool thermal_hardware_set_peltier_heat(double power);
bool thermal_hardware_set_peltier_cool(double power);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMAL_HARDWARE_H_ */
