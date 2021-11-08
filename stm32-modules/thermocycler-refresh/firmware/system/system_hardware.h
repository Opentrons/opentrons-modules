#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#include "stm32g4xx_hal_gpio.h"
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define DBG_LED_PIN GPIO_PIN_6
#define DBG_LED_PORT GPIOE

void system_hardware_setup(void);
void system_debug_led(int set);
void system_hardware_enter_bootloader(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
