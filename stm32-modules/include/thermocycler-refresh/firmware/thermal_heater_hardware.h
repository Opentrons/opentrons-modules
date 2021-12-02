#ifndef THERMAL_HEATER_HARDWARE_H__
#define THERMAL_HEATER_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>

/**
 * @brief Initialize the heater on the board. This is a prerequisite
 * for controlling the heater.
 */
void thermal_heater_initialize(void);

/**
 * @brief Set the power level of the heater. Automatically updates
 * the enable pin.
 *
 * @param[in] power The power to set, as a percentage from 0.0F to 1.0F
 * inclusive. Values outside of this range will be clamped
 * @return True if the fan could be set to \p power, false if an error
 * occurred.
 */
bool thermal_heater_set_power(double power);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMAL_HEATER_HARDWARE_H__ */
