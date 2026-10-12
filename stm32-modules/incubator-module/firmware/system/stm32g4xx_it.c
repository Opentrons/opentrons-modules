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
#include "firmware/serial_hardware.h"
#include "task.h"

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim20;
extern TIM_HandleTypeDef htim8;

static motor_interrupt_callback interrupt_callback = NULL;


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

void USART2_IRQHandler(void)
{
  serial_hardware_irq_handler();
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/* EXTI2 (PC2) is handled in proximity_sensor.c.                              */
/******************************************************************************/


/**
 * TIM1 = step interrupt for motor R
 * TIM20 = step interrupt for motor Z
 * TIM8 = step interrupt for motor P
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7) {
      HAL_IncTick();
    }
    else if(htim->Instance == TIM1 && interrupt_callback != NULL) {
        interrupt_callback(MOTOR_R);
    } else if (htim->Instance == TIM20 && interrupt_callback != NULL) {
        interrupt_callback(MOTOR_Z);
    } else if (htim->Instance == TIM8 && interrupt_callback != NULL) {
        interrupt_callback(MOTOR_P);
    }
}

void initialize_callbacks(motor_interrupt_callback callback_glue) {
    interrupt_callback = callback_glue;
}
