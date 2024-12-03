#ifndef THERMAL_HARDWARE_H_
#define THERMAL_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the hardware involved in the peltier drive circuit.
 * Includes:
 * - 12v enable line
 * - Peltier enable line
 * - TIM1 Ch0 and Ch1 as PWM output for cooling and heating
 *
 */
void thermal_hardware_init();

/**
 * @brief Enable the peltiers. If the peltiers are already enabled,
 * this will have no real effect.
 *
 */
void thermal_hardware_enable_peltiers();
/**
 * @brief Disable the peltiers. This will reset the PWM of each channel to
 * 0 and disable the Peltier Enable line.
 *
 */
void thermal_hardware_disable_peltiers();

/**
 * @brief Set the peltier to drive at a fixed PWM in the heating direction.
 *
 * @param power The percentage of power to apply to the peltier.
 * @return true if the peltier could be set, false if there is an error.
 */
bool thermal_hardware_set_peltier_heat(double power);

/**
 * @brief Set the peltier to drive at a fixed PWM in the cooling direction.
 *
 * @param power The percentage of power to apply to the peltier.
 * @return true if the peltier could be set, false if there is an error.
 */
bool thermal_hardware_set_peltier_cool(double power);

bool thermal_hardware_set_fan_power(double power);

void thermal_hardware_set_eeprom_write_protect(bool set);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMAL_HARDWARE_H_ */
