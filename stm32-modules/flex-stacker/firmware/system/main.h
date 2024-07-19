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
#define HOPPER_DOR_CLOSED_Pin GPIO_PIN_13
#define HOPPER_DOR_CLOSED_GPIO_Port GPIOC

#define MOTOR_DIR_Z_Pin GPIO_PIN_1
#define MOTOR_DIR_Z_GPIO_Port GPIOC
#define MOTOR_STEP_Z_Pin GPIO_PIN_2
#define MOTOR_STEP_Z_GPIO_Port GPIOC
#define LIMIT_Z__Pin GPIO_PIN_3
#define LIMIT_Z__GPIO_Port GPIOC
#define LIMIT_Z_A0_Pin GPIO_PIN_0
#define LIMIT_Z_A0_GPIO_Port GPIOA
#define LIMIT_X__Pin GPIO_PIN_1
#define LIMIT_X__GPIO_Port GPIOA
#define LIMIT_X_A2_Pin GPIO_PIN_2
#define LIMIT_X_A2_GPIO_Port GPIOA
#define MOTOR_EN_Z_Pin GPIO_PIN_3
#define MOTOR_EN_Z_GPIO_Port GPIOA
#define MOTOR_EN_X_Pin GPIO_PIN_4
#define MOTOR_EN_X_GPIO_Port GPIOA

#define MOTOR_DIR_X_Pin GPIO_PIN_6
#define MOTOR_DIR_X_GPIO_Port GPIOA
#define MOTOR_STEP_X_Pin GPIO_PIN_7
#define MOTOR_STEP_X_GPIO_Port GPIOA
#define nMOTOR_DIAG1_Pin GPIO_PIN_4
#define nMOTOR_DIAG1_GPIO_Port GPIOC
#define MOTOR_EN_L_Pin GPIO_PIN_5
#define MOTOR_EN_L_GPIO_Port GPIOC
#define MOTOR_DR_L_Pin GPIO_PIN_0
#define MOTOR_DR_L_GPIO_Port GPIOB
#define MOTOR_STEP_L_Pin GPIO_PIN_1
#define MOTOR_STEP_L_GPIO_Port GPIOB

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
#define EEPOM_I2C2_SCL_Pin GPIO_PIN_9
#define EEPOM_I2C2_SCL_GPIO_Port GPIOA
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

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
