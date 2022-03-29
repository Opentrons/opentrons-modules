/**
 * @file board_revision_hardware.h
 * @brief Header for platform-specific board revision reading.
 *
 */
#ifndef BOARD_REVISION_HARDWARE_H_
#define BOARD_REVISION_HARDWARE_H_
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include "systemwide.h"

/**
 * @brief Reads all of the inputs indicating board revision
 * @param[out] inputs Returns the values of the input pins
 */
void board_revision_read_inputs(TrinaryInput_t *inputs);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* BOARD_REVISION_HARDWARE_H_ */
