#include "stm32f3xx_hal.h"
#include "uart_hardware.h"
#include "FreeRTOS.h"
#include "task.h"

static UART_HandleTypeDef *uart_handle;

void UART_Init(UART_HandleTypeDef *huart)
{
  HAL_StatusTypeDef ret;
  uart_handle = huart;

  // huart state does not need to be preset to RESET because the call to
  // DeInit the peripheral handles that
  huart->Instance        = USARTx;
  huart->Init.BaudRate   = 115200;
  huart->Init.WordLength = UART_WORDLENGTH_8B;
  huart->Init.StopBits   = UART_STOPBITS_1;
  huart->Init.Parity     = UART_PARITY_NONE;
  huart->Init.Mode       = UART_MODE_TX_RX;
  huart->Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  ret = HAL_UART_DeInit(huart);
  configASSERT(ret == HAL_OK);
  if (ret == HAL_OK) {
    ret = HAL_UART_Init(huart);
    configASSERT(ret == HAL_OK);
  }

  /* Configure NVIC */
  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void UART_DeInit(UART_HandleTypeDef *huart)
{
  HAL_StatusTypeDef ret;

  huart->Instance        = USARTx;
  huart->Init.BaudRate   = 115200;
  huart->Init.WordLength = UART_WORDLENGTH_8B;
  huart->Init.StopBits   = UART_STOPBITS_1;
  huart->Init.Parity     = UART_PARITY_NONE;
  huart->Init.Mode       = UART_MODE_TX_RX;
  huart->Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  ret = HAL_UART_DeInit(huart);
  configASSERT(ret == HAL_OK);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  USARTx_TX_GPIO_CLK_ENABLE();
  USARTx_RX_GPIO_CLK_ENABLE();

  /* Enable USARTx clock */
  USARTx_CLK_ENABLE(); 
  
  /*##-2- Configure peripheral GPIO ##########################################*/  
  /* UART TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = USARTx_TX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = USARTx_AF;

  HAL_GPIO_Init(USARTx_GPIO_PORT, &GPIO_InitStruct);

  /* UART RX GPIO pin configuration  */
  GPIO_InitStruct.Pin = USARTx_RX_PIN;
  GPIO_InitStruct.Alternate = USARTx_AF;

  HAL_GPIO_Init(USARTx_GPIO_PORT, &GPIO_InitStruct);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  /*##-1- Reset peripherals ##################################################*/
  USARTx_FORCE_RESET();
  USARTx_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure UART Tx as alternate function  */
  HAL_GPIO_DeInit(USARTx_GPIO_PORT, USARTx_TX_PIN);
  /* Configure UART Rx as alternate function  */
  HAL_GPIO_DeInit(USARTx_GPIO_PORT, USARTx_RX_PIN);
  
  /*##-3- Disable the NVIC for UART ##########################################*/
  HAL_NVIC_DisableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(uart_handle);
}
