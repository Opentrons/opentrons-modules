/**
 * @file motor_spi_hardware.c
 * @brief SPI interface to communicate with the Trinamic TMC2130 motor driver
 */

#include "firmware/motor_spi_hardware.h"
// HAL includes
#include "stm32g4xx_hal_dma.h" // Required to link to hal_spi
#include "stm32g4xx_hal_spi.h"
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

/** Get a single byte out of a 64 bit value. Higher values are
 *  more significant (0 = LSB, 3 = MSB)*/
#define GET_BYTE(val, byte) ((uint8_t)((val >> (byte * 8)) & 0xFF))
/** Move a single byte to fit into a 64 bit value. Higher \c byte
 * values are more significant (0 = LSB, 3 = MSB).*/
#define SET_BYTE(val, byte) (((uint64_t)val) << (byte * 8))

struct motor_spi_hardware {
    SPI_HandleTypeDef handle;
    TaskHandle_t task_to_notify;
    bool initialized;
};

// STATIC VARIABLES
static struct motor_spi_hardware _spi = {
    .handle = {0},
    .task_to_notify =  NULL,
    .initialized = false
};

// STATIC FUNCTION DEFINITIONS
static void spi_interrupt_service(void);

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
        _spi.handle.Init.NSS = SPI_NSS_HARD_OUTPUT;
        _spi.handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
        _spi.handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
        _spi.handle.Init.TIMode = SPI_TIMODE_DISABLE;
        _spi.handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        _spi.handle.Init.CRCPolynomial = 7;
        _spi.handle.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
        _spi.handle.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        ret = HAL_SPI_Init(&_spi.handle);
        configASSERT(ret == HAL_OK);
        _spi.initialized = true;
    }
}

bool motor_spi_sendreceive(uint8_t *in, uint8_t *out, size_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef ret;
    uint32_t notification_val = 0;

    if(!_spi.initialized || (_spi.task_to_notify != NULL)) { 
        return false;
    }

    _spi.task_to_notify = xTaskGetCurrentTaskHandle();
    ret = HAL_SPI_TransmitReceive_IT(&_spi.handle, in, out, len);
    if(ret != HAL_OK) {
        _spi.task_to_notify = NULL;
        return false;
    }
    notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    if(notification_val != 1) {
        _spi.task_to_notify = NULL;
        return false;
    }
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
