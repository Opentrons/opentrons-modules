#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Enter the bootloader. This function never returns.
 */
void system_hardware_enter_bootloader(void);
void system_hardware_gpio_init(void);
bool system_hardware_read_door_closed(void);
uint16_t system_hardware_reset_reason(void);
bool system_hardware_read_install_detected(void);
void enable_eeprom_write(bool enable);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
