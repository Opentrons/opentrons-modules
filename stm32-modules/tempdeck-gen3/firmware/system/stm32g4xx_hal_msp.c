/**
 * This file contains STM32 HAL MSP function definitions.
 * These are contained in the same file because they all overwrite weakly
 * defined functions in the HAL and are not directly invoked by any user code.
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/
  /* PendSV_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

  /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
  */
  HAL_PWREx_DisableUCPDDeadBattery();
}

void HAL_MspDeInit(void)
{

    __HAL_RCC_SYSCFG_CLK_DISABLE();
    __HAL_RCC_PWR_CLK_DISABLE();
    
    __HAL_RCC_TIM7_CLK_DISABLE();
    __HAL_RCC_USB_CLK_DISABLE();
    __HAL_RCC_SYSCFG_CLK_DISABLE();
    __HAL_RCC_PWR_CLK_DISABLE();
    /* System interrupt init*/
    /* PendSV_IRQn interrupt configuration */
    HAL_NVIC_DisableIRQ(PendSV_IRQn);
}

/**
* @brief TIM_Base MSP De-Initialization
* @param htim_base: TIM_Base handle pointer
* @retval None
*/
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
    if(htim_base->Instance==TIM7)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM7_CLK_DISABLE();
        /* TIM6 interrupt DeInit */
        HAL_NVIC_DisableIRQ(TIM7_IRQn);
    }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
