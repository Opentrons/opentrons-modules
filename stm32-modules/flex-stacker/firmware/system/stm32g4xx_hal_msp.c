/**
  ******************************************************************************
  * @file         stm32g4xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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

extern DMA_HandleTypeDef hdma_spi2_rx;

extern DMA_HandleTypeDef hdma_spi2_tx;


void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
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

    /* Peripheral interrupt init */
    /* RCC_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(RCC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(RCC_IRQn);

    /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
     */
    HAL_PWREx_DisableUCPDDeadBattery();
}

/**
* @brief I2C MSP Initialization
* This function configures the hardware resources used in this example
* @param hi2c: I2C handle pointer
* @retval None
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(hi2c->Instance==I2C2)
    {
        /** Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
        PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**I2C2 GPIO Configuration
        PA8     ------> I2C2_SDA
        PA9     ------> I2C2_SCL
        */
        GPIO_InitStruct.Pin = EEPROM_I2C2_SDA_Pin|EEPOM_I2C2_SCL_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
    else if(hi2c->Instance==I2C3)
    {
        /** Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
        PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_RCC_GPIOC_CLK_ENABLE();
        /**I2C3 GPIO Configuration
        PC8     ------> I2C3_SCL
        PC9     ------> I2C3_SDA
        */
        GPIO_InitStruct.Pin = TOF_I2C3_SCL_Pin|TOF_I2C3_SDA_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF8_I2C3;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C3_CLK_ENABLE();
    }

}

/**
* @brief I2C MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hi2c: I2C handle pointer
* @retval None
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if(hi2c->Instance==I2C2)
    {
        /* Peripheral clock disable */
        __HAL_RCC_I2C2_CLK_DISABLE();

        /**I2C2 GPIO Configuration
        PA8     ------> I2C2_SDA
        PA9     ------> I2C2_SCL
        */
        HAL_GPIO_DeInit(EEPROM_I2C2_SDA_GPIO_Port, EEPROM_I2C2_SDA_Pin);

        HAL_GPIO_DeInit(EEPOM_I2C2_SCL_GPIO_Port, EEPOM_I2C2_SCL_Pin);
    }
    else if(hi2c->Instance==I2C3)
    {
        /* Peripheral clock disable */
        __HAL_RCC_I2C3_CLK_DISABLE();

        /**I2C3 GPIO Configuration
        PC8     ------> I2C3_SCL
        PC9     ------> I2C3_SDA
        */
        HAL_GPIO_DeInit(TOF_I2C3_SCL_GPIO_Port, TOF_I2C3_SCL_Pin);

        HAL_GPIO_DeInit(TOF_I2C3_SDA_GPIO_Port, TOF_I2C3_SDA_Pin);
    }

}

/**
* @brief UART MSP Initialization
* This function configures the hardware resources used in this example
* @param huart: UART handle pointer
* @retval None
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(huart->Instance==LPUART1)
    {
        /** Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
        PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }

        /* Peripheral clock enable */
        __HAL_RCC_LPUART1_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        /**LPUART1 GPIO Configuration
        PB10     ------> LPUART1_RX
        PB11     ------> LPUART1_TX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF8_LPUART1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }

}

/**
* @brief UART MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param huart: UART handle pointer
* @retval None
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
    if(huart->Instance==LPUART1)
    {
        /* Peripheral clock disable */
        __HAL_RCC_LPUART1_CLK_DISABLE();

        /**LPUART1 GPIO Configuration
        PB10     ------> LPUART1_RX
        PB11     ------> LPUART1_TX
        */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);
    }

}



/**
* @brief TIM_Base MSP Initialization
* This function configures the hardware resources used in this example
* @param htim_base: TIM_Base handle pointer
* @retval None
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
    if(htim_base->Instance==TIM3)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
    else if(htim_base->Instance==TIM17)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM17_CLK_ENABLE();
    }
    else if(htim_base->Instance==TIM20)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM20_CLK_ENABLE();
    }

}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(htim->Instance==TIM3)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /**TIM3 GPIO Configuration
        PB1     ------> TIM3_CH4
        */
        GPIO_InitStruct.Pin = MOTOR_STEP_L_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
        HAL_GPIO_Init(MOTOR_STEP_L_GPIO_Port, &GPIO_InitStruct);

    }
    else if(htim->Instance==TIM17)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**TIM17 GPIO Configuration
        PA7     ------> TIM17_CH1
        */
        GPIO_InitStruct.Pin = MOTOR_STEP_X_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM17;
        HAL_GPIO_Init(MOTOR_STEP_X_GPIO_Port, &GPIO_InitStruct);

    }
    else if(htim->Instance==TIM20)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        /**TIM20 GPIO Configuration
        PC2     ------> TIM20_CH2
        */
        GPIO_InitStruct.Pin = MOTOR_STEP_Z_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF6_TIM20;
        HAL_GPIO_Init(MOTOR_STEP_Z_GPIO_Port, &GPIO_InitStruct);
    }

}
/**
* @brief TIM_Base MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param htim_base: TIM_Base handle pointer
* @retval None
 */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
    if(htim_base->Instance==TIM3)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM3_CLK_DISABLE();
    }
    else if(htim_base->Instance==TIM17)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM17_CLK_DISABLE();
    }
    else if(htim_base->Instance==TIM20)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM20_CLK_DISABLE();
    }

}
