#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

/* The Following variables sometimes complain since some are unused
 * some are only used in the c side or just the c++
 */

#pragma GCC diagnostic push
// NOLINTNEXTLINE(clang-diagnostic-unknown-warning-option)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic pop

#define n48V_FAULT_Pin GPIO_PIN_6
#define n48V_FAULT_GPIO_Port GPIOC
#define nGP_INT_Pin GPIO_PIN_7
#define nGP_INT_GPIO_Port GPIOC

#define EEPROM_I2C2_SDA_Pin GPIO_PIN_8
#define EEPROM_I2C2_SDA_GPIO_Port GPIOA
#define EEPROM_I2C2_SCL_Pin GPIO_PIN_9
#define EEPROM_I2C2_SCL_GPIO_Port GPIOA
#define EEPROM_WP_PIN GPIO_PIN_10
#define EEPROM_WP_PORT GPIOA

#define nSTATUS_LED_Pin GPIO_PIN_10
#define nSTATUS_LED_GPIO_Port GPIOC
