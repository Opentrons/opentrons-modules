/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/

#define n48V_FAULT_Pin GPIO_PIN_6
#define n48V_FAULT_GPIO_Port GPIOC

#define I2C1_SDA_Pin GPIO_PIN_7
#define I2C1_SDA_GPIO_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_15
#define I2C1_SCL_GPIO_Port GPIOA

#define I2C2_SDA_Pin GPIO_PIN_8
#define I2C2_SDA_GPIO_Port GPIOA
#define I2C2_SCL_Pin GPIO_PIN_9
#define I2C2_SCL_GPIO_Port GPIOA

#define I2C3_SCL_Pin GPIO_PIN_8
#define I2C3_SCL_GPIO_Port GPIOC
#define I2C3_SDA_Pin GPIO_PIN_9
#define I2C3_SDA_GPIO_Port GPIOC

// PB6
#define SENSOR_A_RESET_PIN GPIO_PIN_6
#define SENSOR_A_RESET_PORT GPIOB
// PA1
#define SENSOR_A_EOC_PIN GPIO_PIN_1
#define SENSOR_A_EOC_PORT GPIOA
// PC7
#define SENSOR_B_RESET_PIN GPIO_PIN_7
#define SENSOR_B_RESET_PORT GPIOC
// PC3
#define SENSOR_B_EOC_PIN GPIO_PIN_3
#define SENSOR_B_EOC_PORT GPIOC
// PB0
#define ATMOSPHERIC_EOC_PIN GPIO_PIN_0
#define ATMOSPHERIC_EOC_PORT GPIOB

#define EEPROM_WP_PIN GPIO_PIN_10
#define EEPROM_WP_PORT GPIOA

#define nSTATUS_LED_Pin GPIO_PIN_10
#define nSTATUS_LED_GPIO_Port GPIOC
#define USB_VBUS_MCU_Pin GPIO_PIN_4
#define USB_VBUS_MCU_GPIO_Port GPIOB

#define nSTATUS_LED_Pin GPIO_PIN_10
#define nSTATUS_LED_GPIO_Port GPIOC
#define USB_VBUS_MCU_Pin GPIO_PIN_4
#define USB_VBUS_MCU_GPIO_Port GPIOB

// Vent
#define nSLEEP_DRV_MCU_GPIO_Pin GPIO_PIN_3
#define nSLEEP_DRV_MCU_GPIO_Port GPIOA
#define VENT_DAC_MCU_GPIO_Pin GPIO_PIN_4
#define VENT_DAC_MCU_GPIO_Port GPIOA
#define VENT_IN_GPIO_Pin GPIO_PIN_5
#define VENT_IN_GPIO_Port GPIOA
#define VENT_FAULT_GPIO_Pin GPIO_PIN_6
#define VENT_FAULT_GPIO_Port GPIOA

// Pump
#define PUMP_PWM_GPIO_Pin GPIO_PIN_7
#define PUMP_PWM_GPIO_Port GPIOA
#define PUMP_TACH_GPIO_Pin GPIO_PIN_1
#define PUMP_TACH_GPIO_Port GPIOB
#define PUMP_HALL_EN_GPIO_Pin GPIO_PIN_15
#define PUMP_HALL_EN_GPIO_Port GPIOC
#define PUMP_MOTOR_EN_GPIO_Pin GPIO_PIN_2
#define PUMP_MOTOR_EN_GPIO_Port GPIOD
#define PUMP_NTC_GPIO_Pin GPIO_PIN_1
#define PUMP_NTC_GPIO_Port GPIOF
#define PUMP_NFault_GPIO_Pin GPIO_PIN_14
#define PUMP_NFault_GPIO_Port GPIOC

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
