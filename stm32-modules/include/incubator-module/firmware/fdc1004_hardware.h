#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "firmware/i2c_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

bool fdc1004_hardware_init(I2C_BUS bus, uint8_t channel, uint8_t capdac);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus