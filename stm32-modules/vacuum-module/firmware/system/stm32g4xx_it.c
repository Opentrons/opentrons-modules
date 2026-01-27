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

#include "firmware/pump_hardware.h"

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;


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

void EXTI9_5_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_7)) {
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
    }
}

// IRQHandler for sensor a eoc
void EXTI1_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_1)) {
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
    }
}

// IRQHandler for sensor b eoc
void EXTI3_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_3)) {
        HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
    }
}

/**
 * TIM7 = timebase counter
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM7) {
        HAL_IncTick();
    } else if(htim->Instance == TIM3) {
       tach_period_overflow_callback();
    }

}

/**
 * @brief This function handles TIM3 global interrupt.
 */
__attribute__((section(".ccmram")))
void TIM3_IRQHandler(void) { HAL_TIM_IRQHandler(&htim3); }

