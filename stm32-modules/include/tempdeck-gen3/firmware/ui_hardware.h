#ifndef UI_HARDWARE_H__
#define UI_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>

void ui_hardware_initialize();

/**
 * @brief Enter the bootloader. This function never returns.
 */
void ui_hardware_set_heartbeat_led(bool setting);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _UI_HARDWARE_H__
