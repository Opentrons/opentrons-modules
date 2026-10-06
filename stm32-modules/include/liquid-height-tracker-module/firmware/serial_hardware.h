#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

bool serial_hardware_init(void);
bool serial_hardware_write(const char *data, size_t length);
bool serial_hardware_read_byte(uint8_t *byte);
void serial_hardware_irq_handler(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus