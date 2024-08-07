/**
 ******************************************************************************
 * @file    stm32g4xx_it.h
 * @brief   This file contains the headers of the interrupt handlers.
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
#ifndef __STM32G4xx_IT_H
#define __STM32G4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "systemwide.h"

/* Exported functions prototypes ---------------------------------------------*/
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
// void SysTick_Handler(void);
void RCC_IRQHandler(void);
// void DMA1_Channel1_IRQHandler(void);
// void DMA1_Channel2_IRQHandler(void);

typedef void (*motor_interrupt_callback)(MotorID motor_id);
void initialize_callbacks(motor_interrupt_callback callback_glue);
#ifdef __cplusplus
}
#endif

#endif /* __STM32G4xx_IT_H */
