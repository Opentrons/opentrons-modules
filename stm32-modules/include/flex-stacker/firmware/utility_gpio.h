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
#define TOF_I2C3_SCL_Pin GPIO_PIN_8
#define TOF_I2C3_SCL_GPIO_Port GPIOC
#define TOF_I2C3_SDA_Pin GPIO_PIN_9
#define TOF_I2C3_SDA_GPIO_Port GPIOC

#define EEPROM_I2C2_SDA_Pin GPIO_PIN_8
#define EEPROM_I2C2_SDA_GPIO_Port GPIOA
#define EEPROM_I2C2_SCL_Pin GPIO_PIN_9
#define EEPROM_I2C2_SCL_GPIO_Port GPIOA
#define EEPROM_WP_PIN GPIO_PIN_10
#define EEPROM_WP_PORT GPIOA

#define nSTATUS_LED_Pin GPIO_PIN_10
#define nSTATUS_LED_GPIO_Port GPIOC
#define nLW_RELEASED_Pin GPIO_PIN_11
#define nLW_RELEASED_GPIO_Port GPIOC
#define USB_VBUS_MCU_Pin GPIO_PIN_4
#define USB_VBUS_MCU_GPIO_Port GPIOB
#define nLW_HELD_Pin GPIO_PIN_5
#define nLW_HELD_GPIO_Port GPIOB
#define nESTOP_Pin GPIO_PIN_6
#define nESTOP_GPIO_Port GPIOB
#define nBRAKE_Z_Pin GPIO_PIN_7
#define nBRAKE_Z_GPIO_Port GPIOB
#define nBRAKE_X_Pin GPIO_PIN_9
#define nBRAKE_X_GPIO_Port GPIOB

