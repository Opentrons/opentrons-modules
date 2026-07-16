#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Enter the bootloader. This function never returns.
 *
 * Disconnects USB and reprograms nSWBOOT0/nBOOT0 so the next reset boots the
 * STM system-memory DFU bootloader (verified working path on this hardware;
 * a pure software jump does not enumerate USB DFU on the vacuum module).
 */
void system_hardware_enter_bootloader(void);

/**
 * @brief If option bytes still select system-memory boot (left over from DFU
 * entry), restore main-flash boot. Call early in HardwareInit. May reset.
 */
void system_hardware_handle_bootloader_request(void);

void system_hardware_gpio_init(void);
uint16_t system_hardware_reset_reason(void);
void enable_eeprom_write(bool enable);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
