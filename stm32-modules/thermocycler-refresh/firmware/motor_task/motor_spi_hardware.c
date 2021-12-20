/**
 * @file motor_spi_hardware.c
 * @brief SPI interface to communicate with the Trinamic TMC2130 motor driver
 */

#include "firmware/motor_spi_hardware.h"
// System includes
#include <string.h>
// HAL includes
#include "stm32g4xx_hal_dma.h" // Required to link to hal_spi
#include "stm32g4xx_hal_spi.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_rcc.h"
// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

// LOCAL DEFINES

/** Flag to mark a SPI Write to the chip.*/
#define FLAG_WRITE  (0x80)
/** Flag to mark a SPI Read to the chip.*/
#define FLAG_READ   (0x00)
/** Empty byte to write when reading.*/
#define EMPTY_WRITE (0x00)
/** Length of an entire message is 5 bytes: status byte + 4 register bytes.*/
#define TMC_MESSAGE_SIZE (1 + 4)
/** Default register to read from if we don't care about return.*/
#define READ_REG_DEFAULT (0x00 | FLAG_READ)

/** Port for the enable pin.*/
#define MOTOR_SPI_ENABLE_PORT (GPIOE)
/** Pin for enable pin.*/
#define MOTOR_SPI_ENABLE_PIN (GPIO_PIN_15)

/** Port for NSS pin.*/
#define MOTOR_SPI_NSS_PORT (GPIOD)
/** Pin for NSS pin.*/
#define MOTOR_SPI_NSS_PIN (GPIO_PIN_15)
/** Maximum length of a SPI transaction is 5 bytes.*/
#define MOTOR_MAX_SPI_LEN (5)

/** Get a single byte out of a 64 bit value. Higher values are
 *  more significant (0 = LSB, 3 = MSB)*/
#define GET_BYTE(val, byte) ((uint8_t)((val >> (byte * 8)) & 0xFF))
/** Move a single byte to fit into a 64 bit value. Higher \c byte
 * values are more significant (0 = LSB, 3 = MSB).*/
#define SET_BYTE(val, byte) (((uint64_t)val) << (byte * 8))

struct motor_spi_hardware {
    SPI_HandleTypeDef handle;
    TaskHandle_t task_to_notify;
    bool enabled;
    bool initialized;
};

// STATIC VARIABLES
static struct motor_spi_hardware _spi = {
    .handle = {0},
    .task_to_notify =  NULL,
    .enabled = false,
    .initialized = false
};

// STATIC FUNCTION DEFINITIONS
static void spi_interrupt_service(void);
static void spi_set_nss(bool selected);

// PUBLIC FUNCTION IMPLEMENTATION

void motor_spi_initialize(void) {
    if(!_spi.initialized) {
        HAL_StatusTypeDef ret;
        _spi.handle.Instance = SPI2;
        _spi.handle.Init.Mode = SPI_MODE_MASTER;
        _spi.handle.Init.Direction = SPI_DIRECTION_2LINES;
        _spi.handle.Init.DataSize = SPI_DATASIZE_8BIT;
        _spi.handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
        _spi.handle.Init.CLKPhase = SPI_PHASE_2EDGE;
        // Hardware NSS behavior is irregular so we disable it
        _spi.handle.Init.NSS = SPI_NSS_SOFT;
        _spi.handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
        _spi.handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
        _spi.handle.Init.TIMode = SPI_TIMODE_DISABLE;
        _spi.handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        _spi.handle.Init.CRCPolynomial = 7;
        _spi.handle.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
        _spi.handle.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        ret = HAL_SPI_Init(&_spi.handle);
        configASSERT(ret == HAL_OK);

        // Initialize the GPIO
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin = MOTOR_SPI_ENABLE_PIN;
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_LOW;
        HAL_GPIO_Init(MOTOR_SPI_ENABLE_PORT, &gpio);
        _spi.initialized = true;

        __HAL_RCC_GPIOD_CLK_ENABLE();
        gpio.Pin = MOTOR_SPI_NSS_PIN;
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_LOW;
        HAL_GPIO_Init(MOTOR_SPI_NSS_PORT, &gpio);
        spi_set_nss(false);

        motor_set_output_enable(false);
    }
}

bool motor_spi_sendreceive(uint8_t *in, uint8_t *out, size_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef ret;
    uint32_t notification_val = 0;

    if(!_spi.initialized || (_spi.task_to_notify != NULL) || (len > MOTOR_MAX_SPI_LEN)) { 
        return false;
    }
    spi_set_nss(true);
    _spi.task_to_notify = xTaskGetCurrentTaskHandle();
    ret = HAL_SPI_TransmitReceive_IT(&_spi.handle, in, out, len);
    if(ret != HAL_OK) {
        _spi.task_to_notify = NULL;
        return false;
    }
    notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    spi_set_nss(false);
    // If the task was preempted by the error handler rather than the
    // TxRx complete callback, the remaining count should be greater
    // than 0.
    if((notification_val != 1) || (_spi.handle.RxXferCount > 0)) {
        _spi.task_to_notify = NULL;
        return true;
    }
    return true;
}

bool motor_set_output_enable(bool enable) {
    if(!_spi.initialized) { return false; }
    _spi.enabled = enable;
    HAL_GPIO_WritePin(MOTOR_SPI_ENABLE_PORT, MOTOR_SPI_ENABLE_PIN, 
        (enable) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return true;
}

void SPI2_IRQHandler(void)
{
  /* USER CODE BEGIN SPI2_IRQn 0 */

  /* USER CODE END SPI2_IRQn 0 */
  HAL_SPI_IRQHandler(&_spi.handle);
  /* USER CODE BEGIN SPI2_IRQn 1 */

  /* USER CODE END SPI2_IRQn 1 */
}

// STATIC FUNCTION IMPLEMENTATION

static void spi_interrupt_service(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if( _spi.task_to_notify == NULL ) {
        return;
    }
    vTaskNotifyGiveFromISR( _spi.task_to_notify, &xHigherPriorityTaskWoken );
    _spi.task_to_notify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void spi_set_nss(bool selected) {
    HAL_GPIO_WritePin(MOTOR_SPI_NSS_PORT, MOTOR_SPI_NSS_PIN, 
        (selected) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief Overwritten HAL function for SPI TxRx Complete Callback.
 * @details If a task is blocked waiting for the SPI transaction to finish,
 * this function unblocks that task.
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    spi_interrupt_service();
}

/**
 * @brief Overwritten HAL function for SPI Error Callback.
 * @details If a task is blocked waiting for the SPI transaction to finish,
 * this function unblocks that task.
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    spi_interrupt_service();
}
