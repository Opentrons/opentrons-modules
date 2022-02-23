/**
 ******************************************************************************
 * @file    Templates/Src/stm32g4xx_it.c
 * @author  MCD Application Team
 * @brief   Main Interrupt Service Routines.
 *          This file provides template for all exceptions handler and
 *          peripherals interrupt service routine.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_it.h"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"
#include "stm32g4xx_hal_pcd.h"

#include "firmware/thermal_hardware.h"
#include "firmware/motor_hardware.h"
#include "firmware/system_hardware.h"
#include "firmware/system_led_hardware.h"

/** @addtogroup STM32G4xx_HAL_Examples
 * @{
 */

/** @addtogroup Templates
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim7;

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief   This function handles NMI exception.
 * @param  None
 * @retval None
 */
void NMI_Handler(void) {}

/**
 * @brief  This function handles Hard Fault exception.
 * @param  None
 * @retval None
 */
void HardFault_Handler(void) {
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void) {
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void) {
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void) {
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void) {}

/******************************************************************************/
/*                 STM32G4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32g4xxxx.s).                                             */
/******************************************************************************/


/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
 	HAL_TIM_IRQHandler(&htim7);
}

/**
 * @brief This function handles EXTI9-5 IRQ
 */
void EXTI9_5_IRQHandler(void)
{
    thermal_adc_ready_callback(ADC1_ITR);
}

/**
 * @brief This function handles EXTI10-15 IRQ
 */
void EXTI15_10_IRQHandler(void)
{
    thermal_adc_ready_callback(ADC2_ITR);
    system_front_button_callback();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) 
{
    if(htim->Instance == TIM17) {
        system_led_pulse_callback();
    }
}

void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) 
{
    if(htim->Instance == TIM17) {
        // The half-complete interrupt isn't used for this application
        return;
    }
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        motor_hardware_lid_increment();
    }
}

/**
 * TIM7 = timebase counter
 * 
 * TIM6 = seal motor counter
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM7) {
        hal_timebase_tick();
    } else if(htim->Instance == TIM6) {
        motor_hardware_seal_interrupt();
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
