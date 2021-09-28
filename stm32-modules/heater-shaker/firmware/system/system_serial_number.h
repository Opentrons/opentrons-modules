#ifndef __SYSTEM_SERIAL_NUMBER_H_
#define __SYSTEM_SERIAL_NUMBER_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>

#include "stm32f3xx_hal.h"

bool system_set_serial_number(uint64_t to_write, uint8_t page);

uint64_t system_get_serial_number(uint8_t page);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __SYSTEM_SERIAL_NUMBER_H_