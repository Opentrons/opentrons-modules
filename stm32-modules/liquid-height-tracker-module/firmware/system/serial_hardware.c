#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "firmware/serial_hardware.h"
#include "main.h"
#include "stm32g4xx_hal.h"

#define SERIAL_RX_BUFFER_SIZE 128U

static UART_HandleTypeDef serial_uart;
static bool serial_initialized = false;
static uint8_t serial_rx_byte;
static volatile uint8_t serial_rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint16_t serial_rx_head;
static volatile uint16_t serial_rx_tail;

bool serial_hardware_init(void) {
    serial_uart.Instance = USART2;
    serial_uart.Init.BaudRate = 115200;
    serial_uart.Init.WordLength = UART_WORDLENGTH_8B;
    serial_uart.Init.StopBits = UART_STOPBITS_1;
    serial_uart.Init.Parity = UART_PARITY_NONE;
    serial_uart.Init.Mode = UART_MODE_TX_RX;
    serial_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    serial_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    serial_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    serial_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    serial_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&serial_uart) != HAL_OK) {
        return false;
    }
    serial_initialized = true;
    if (HAL_UART_Receive_IT(&serial_uart, &serial_rx_byte, 1) != HAL_OK) {
        serial_initialized = false;
        return false;
    }
    return true;
}

bool serial_hardware_write(const char *data, size_t length) {
    if (!serial_initialized) {
        return false;
    }

    size_t offset = 0;
    while (offset < length) {
        size_t remaining = length - offset;
        uint16_t chunk_size = remaining > UINT16_MAX ? UINT16_MAX : remaining;
        if (HAL_UART_Transmit(&serial_uart, (uint8_t *)&data[offset],
                              chunk_size, HAL_MAX_DELAY) != HAL_OK) {
            return false;
        }
        offset += chunk_size;
    }
    return true;
}

bool serial_hardware_read_byte(uint8_t *byte) {
    if (!serial_initialized || byte == NULL) {
        return false;
    }
    const uint16_t tail = serial_rx_tail;
    if (tail == serial_rx_head) {
        return false;
    }
    __DMB();
    *byte = serial_rx_buffer[tail];
    serial_rx_tail =
        (uint16_t)((tail + 1U) % SERIAL_RX_BUFFER_SIZE);
    return true;
}

void serial_hardware_irq_handler(void) {
    HAL_UART_IRQHandler(&serial_uart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
    if (uart != &serial_uart) {
        return;
    }

    const uint16_t next_head =
        (uint16_t)((serial_rx_head + 1U) % SERIAL_RX_BUFFER_SIZE);
    if (next_head != serial_rx_tail) {
        serial_rx_buffer[serial_rx_head] = serial_rx_byte;
        __DMB();
        serial_rx_head = next_head;
    }
    (void)HAL_UART_Receive_IT(&serial_uart, &serial_rx_byte, 1);
}

int _write(int file, char *data, int length) {
    (void)file;
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    if (!serial_hardware_write(data, (size_t)length)) {
        errno = EIO;
        return -1;
    }
    return length;
}