#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "firmware/serial_hardware.h"
#include "main.h"
#include "semphr.h"
#include "stm32g4xx_hal.h"
#include "task.h"

#define SERIAL_RX_BUFFER_SIZE 128U
#define SERIAL_TX_LOCK_TIMEOUT_MS 250U

static void serial_rx_restart(void);

static UART_HandleTypeDef serial_uart;
static bool serial_initialized = false;
static StaticSemaphore_t serial_tx_mutex_state;
static SemaphoreHandle_t serial_tx_mutex = NULL;
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
    if (serial_tx_mutex == NULL) {
        serial_tx_mutex = xSemaphoreCreateMutexStatic(&serial_tx_mutex_state);
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

    /* Several tasks share USART2; without the lock HAL returns BUSY and the
     * losing task's line is silently dropped. */
    const bool use_lock = serial_tx_mutex != NULL &&
                          xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (use_lock &&
        xSemaphoreTake(serial_tx_mutex,
                       pdMS_TO_TICKS(SERIAL_TX_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    bool ok = true;
    size_t offset = 0;
    while (offset < length) {
        size_t remaining = length - offset;
        uint16_t chunk_size = remaining > UINT16_MAX ? UINT16_MAX : remaining;
        /* Finite timeout: HAL_MAX_DELAY can wedge forever if the UART clock
         * is wrong on cold boot before ST-LINK VCP is ready. */
        if (HAL_UART_Transmit(&serial_uart, (uint8_t *)&data[offset],
                              chunk_size, 100U) != HAL_OK) {
            ok = false;
            break;
        }
        offset += chunk_size;
    }

    if (use_lock) {
        (void)xSemaphoreGive(serial_tx_mutex);
    }
    serial_rx_restart();
    return ok;
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

static void serial_rx_restart(void) {
    if (serial_uart.RxState == HAL_UART_STATE_READY) {
        (void)HAL_UART_Receive_IT(&serial_uart, &serial_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
    if (uart != &serial_uart) {
        return;
    }
    /* Overrun stops the receive interrupt. Arm it again or commands die
     * while transmit keeps working. */
    serial_rx_restart();
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