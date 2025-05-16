/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32g4xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern TIM_HandleTypeDef htim17;
extern TIM_HandleTypeDef htim20;
extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi2;


motor_interrupt_callback interrupt_callback = NULL;
limit_switch_callback lim_switch_callback = NULL;
/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void)
{
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/******************************************************************************/


/**
 * TIM7 = timebase counter
 * TIM17 = Interrupt for X
 * TIM20 = Interrupt for Z
 * TIM3 = Interrupt for L
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM7) {
        HAL_IncTick();
    } else if(htim->Instance == TIM17 && interrupt_callback) {
        interrupt_callback(MOTOR_X);
    } else if(htim->Instance == TIM20 && interrupt_callback) {
        interrupt_callback(MOTOR_Z);
    } else if(htim->Instance == TIM3 && interrupt_callback) {
        interrupt_callback(MOTOR_L);
    }
}

void initialize_callbacks(motor_interrupt_callback callback_glue) {
    interrupt_callback = callback_glue;
}

void initialize_limit_switch_callbacks(limit_switch_callback callback_glue) {
    lim_switch_callback = callback_glue;
}

// MOTOR_DIAG0_PIN interrupt
void EXTI15_10_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_12)) {
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
    }
}

void EXTI9_5_IRQHandler(void)
{
    // Estop interrupt
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
    }
    // Latch held limit switch interrupt
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
        if (lim_switch_callback) {
            lim_switch_callback(MOTOR_L);
        }

    }
}

// Z+ limit switch interrupt
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_0)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
        if (lim_switch_callback) {
            lim_switch_callback(MOTOR_Z);
        }
    }
}

// X- limit switch interrupt
void EXTI1_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_1)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
        if (lim_switch_callback) {
            lim_switch_callback(MOTOR_X);
        }
    }
}

// X+ limit switch interrupt
void EXTI2_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_2)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
        if (lim_switch_callback) {
            lim_switch_callback(MOTOR_X);
        }
    }
}

// Z- limit switch interrupt
void EXTI3_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_3)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
        if (lim_switch_callback) {
            lim_switch_callback(MOTOR_Z);
        }
    }
}


