/**
 * @file startup_it.c
 * @brief Contains interrupt vector definitions that are required
 * for the STM32 startup app
 * 
 */

#include "startup_hal.h"
#include "startup_jumps.h"

void HardFault_Handler(void) __attribute__((naked));
void MemManage_Handler(void) __attribute__((naked));
void BusFault_Handler(void) __attribute__((naked));
void UsageFault_Handler(void) __attribute__((naked));

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
    /* Go to bootloader when Hard Fault exception occurs */
    jump_from_exception();
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void) {
    /* Go to bootloader when Memory Manage exception occurs */
    jump_from_exception();
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void) {
    /* Go to bootloader when Bus Fault exception occurs */
    jump_from_exception();
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void) {
    /* Go to bootloader when Usage Fault exception occurs */
    jump_from_exception();
}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void) {}

// Interrupts that are normally handled by FreeRTOS 

void SVC_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) {
    HAL_IncTick();
}
