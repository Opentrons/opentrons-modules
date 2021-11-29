#ifndef THERMAL_PELTIER_HARDWARE_H__
#define THERMAL_PELTIER_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

/**
 * @brief Initialize the Peltier hardware on the board. This
 * is a prerequisite to controlling any peltiers.
 */
void thermal_peltier_initialize(void);
/**
 * @brief Enable or disable the peltiers on the board. When the
 * peltiers are disabled, this function also resets the power of
 * each peltier to 0%
 * @param[in] enable True to enable peltiers, false to disable them
 */
void thermal_peltier_set_enable(const bool enable);
/**
 * @brief Check if the peltiers are enabled
 *
 * @return true if they are enabled, false if they are disabled
 */
bool thermal_peltier_get_enable(void);
/**
 * @brief Sets the power of a peltier.
 *
 * @param[in] id The ID of the peltier to control
 * @param[in] power The power to set for this peltier, as a percentage
 * from 0 to 1.0 inclusive.
 * @param[in] direction The direction to control the peltier, either
 * PELTIER_HEATING or PELTIER_COOLING
 */
void thermal_peltier_set_power(const PeltierID id, double power,
                               const PeltierDirection direction);

/**
 * @brief Get the power of a peltier
 * @param[in] id The ID of the peltier to get
 * @param[out] power Returns the power value of the peltier. Ignored
 * if NULL.
 * @param[out] direction Returns the directionality of the peltier.
 * Ignored if NULL.
 * @return True if information could be returned, false otherwise.
 */
bool thermal_peltier_get_power(const PeltierID id, double *power,
                               PeltierDirection *direction);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* THERMAL_PELTIER_HARDWARE_H__ */
