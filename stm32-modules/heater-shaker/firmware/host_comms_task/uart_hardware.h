#ifndef __UART_HARDWARE_H
#define __UART_HARDWARE_H
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_uart.h"
#include "stm32f3xx_hal_uart_ex.h"
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void UART_Init(UART_HandleTypeDef *huart);
void UART_DeInit(UART_HandleTypeDef *huart);
void USART2_IRQHandler(void);

#define UART_BUFFER_MAX_SIZE 64U //sized to capture any incoming gcode commands
#define UART_BUFFER_MIN_SIZE 16U //threshold for determining remaining buffer size insufficient

/* Definition for USARTx clock resources */
#define USARTx USART2
#define USARTx_CLK_ENABLE() __HAL_RCC_USART2_CLK_ENABLE()
#define USARTx_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define USARTx_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()

#define USARTx_FORCE_RESET() __HAL_RCC_USART2_FORCE_RESET()
#define USARTx_RELEASE_RESET() __HAL_RCC_USART2_RELEASE_RESET()

/* Definition for USARTx Pins */
#define USARTx_GPIO_PORT GPIOD
#define USARTx_TX_PIN GPIO_PIN_5
#define USARTx_RX_PIN GPIO_PIN_6
#define USARTx_AF GPIO_AF7_USART2

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __UART_HARDWARE_H
