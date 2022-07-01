#ifndef __SYSTEM_SERIAL_NUMBER_H_
#define __SYSTEM_SERIAL_NUMBER_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct writable_serial {
    uint64_t contents[3];
};

bool system_set_serial_number(struct writable_serial* to_write);

uint64_t system_get_serial_number(uint8_t address);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __SYSTEM_SERIAL_NUMBER_H_
