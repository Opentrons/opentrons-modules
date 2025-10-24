#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void vent_hardware_init(void);
void hw_open_vent(bool open);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
