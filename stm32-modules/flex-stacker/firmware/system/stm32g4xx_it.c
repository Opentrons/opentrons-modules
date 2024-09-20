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
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim20;
extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi2;


motor_interrupt_callback interrupt_callback = NULL;
stallguard_interrupt_callback stallguard_callback = NULL;
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
    else if (htim->Instance == TIM6 && stallguard_callback) {
        stallguard_callback();
    }
}

void initialize_callbacks(motor_interrupt_callback callback_glue) {
    interrupt_callback = callback_glue;
}

void initialize_stallguard_callback(stallguard_interrupt_callback callback_glue) {
    stallguard_callback = callback_glue;
}
