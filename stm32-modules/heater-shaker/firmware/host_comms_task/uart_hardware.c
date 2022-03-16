#include "stm32f3xx_hal.h"
#include "uart_hardware.h"

static UART_HandleTypeDef* uart_handle;

static void Error_Handler(void);

void UART_Init(UART_HandleTypeDef *huart)
{
  uart_handle = huart;

  huart->Instance        = USARTx;

  huart->Init.BaudRate   = 115200; //to match default STM32CubeIDE VCP setting
  huart->Init.WordLength = UART_WORDLENGTH_8B;
  huart->Init.StopBits   = UART_STOPBITS_1;
  huart->Init.Parity     = UART_PARITY_NONE;
  huart->Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  huart->Init.Mode       = UART_MODE_TX_RX;
  huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT; 
  if(HAL_UART_DeInit(huart) != HAL_OK)
  {
    Error_Handler();
  }  
  if(HAL_UART_Init(huart) != HAL_OK)
  {
    Error_Handler();
  }
}

void UART_DeInit(UART_HandleTypeDef *huart)
{
  huart->Instance        = USARTx;

  huart->Init.BaudRate   = 9600;
  huart->Init.WordLength = UART_WORDLENGTH_8B;
  huart->Init.StopBits   = UART_STOPBITS_1;
  huart->Init.Parity     = UART_PARITY_NONE;
  huart->Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  huart->Init.Mode       = UART_MODE_TX_RX;
  huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT; 
  if(HAL_UART_DeInit(huart) != HAL_OK)
  {
    Error_Handler();
  }  
}

static void Error_Handler(void)
{
  while(1)
  {
  }  
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
  GPIO_InitStruct.Alternate = USARTx_TX_AF;

  HAL_GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

  /* UART RX GPIO pin configuration  */
  GPIO_InitStruct.Pin = USARTx_RX_PIN;
  GPIO_InitStruct.Alternate = USARTx_RX_AF;

  HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);
    
  /*##-3- Configure the NVIC for UART ########################################*/
  /* NVIC for USART */
  HAL_NVIC_SetPriority(USARTx_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(USARTx_IRQn);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  /*##-1- Reset peripherals ##################################################*/
  USARTx_FORCE_RESET();
  USARTx_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure UART Tx as alternate function  */
  HAL_GPIO_DeInit(USARTx_TX_GPIO_PORT, USARTx_TX_PIN);
  /* Configure UART Rx as alternate function  */
  HAL_GPIO_DeInit(USARTx_RX_GPIO_PORT, USARTx_RX_PIN);
  
  /*##-3- Disable the NVIC for UART ##########################################*/
  HAL_NVIC_DisableIRQ(USARTx_IRQn);
}

void USARTx_IRQHandler(void)
{
  HAL_UART_IRQHandler(uart_handle);
}
