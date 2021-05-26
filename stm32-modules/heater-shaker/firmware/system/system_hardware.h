#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#include "stm32f3xx_hal_gpio.h"
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define SOFTPOWER_BUTTON_SENSE_PIN GPIO_PIN_4
#define SOFTPOWER_UNPLUG_SENSE_PIN GPIO_PIN_5
#define SOFTPOWER_PORT GPIOB

void system_hardware_setup(void);
void system_hardware_enter_bootloader(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
