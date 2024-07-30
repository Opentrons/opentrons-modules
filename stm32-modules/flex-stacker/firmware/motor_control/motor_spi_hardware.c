#include "firmware/motor_hardware.h"
#include "systemwide.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_spi.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_dma.h"

#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>


/* SPI2 Pins */
#define SPI2_SCK_Pin (GPIO_PIN_13)
#define SPI2_CIPO_Pin (GPIO_PIN_14)
#define SPI2_COPI_Pin (GPIO_PIN_15)
#define SPI2_Port (GPIOB)

/* SPI2 NSS for the motor Z */
#define nSPI2_NSS_Z_Pin (GPIO_PIN_0)
#define nSPI2_NSS_Z_GPIO_Port (GPIOC)
/* SPI2 NSS for the motor X */
#define nSPI2_NSS_X_Pin (GPIO_PIN_5)
#define nSPI2_NSS_X_GPIO_Port (GPIOA)
/* SPI2 nNSS for the motor L */
#define nSPI2_NSS_L_Pin (GPIO_PIN_2)
#define nSPI2_NSS_L_GPIO_Port (GPIOB)

/** Maximum length of a SPI transaction is 5 bytes.*/
#define MOTOR_MAX_SPI_LEN (5)


/** Static Variables -------------------------------------------------------- */

struct motor_spi_hardware {
    SPI_HandleTypeDef handle;
    TaskHandle_t task_to_notify;
    bool initialized;
};

static void spi_interrupt_service(void);
static void enable_spi_nss(MotorID motor);
static void disable_spi_nss(void);


DMA_HandleTypeDef hdma_spi2_rx;
DMA_HandleTypeDef hdma_spi2_tx;


static struct motor_spi_hardware _spi = {
    .handle = {0},
    .task_to_notify = NULL,
    .initialized = false
};

/** Private Functions ------------------------------------------------------- */

static void disable_spi_nss(void) {
    HAL_GPIO_WritePin(nSPI2_NSS_Z_GPIO_Port, nSPI2_NSS_Z_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(nSPI2_NSS_X_GPIO_Port, nSPI2_NSS_X_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(nSPI2_NSS_L_GPIO_Port, nSPI2_NSS_L_Pin, GPIO_PIN_RESET);
}

static void enable_spi_nss(MotorID motor) {
    // Make sure all NSS pins are disabled
    disable_spi_nss();

    switch(motor) {
        case MOTOR_Z:
            HAL_GPIO_WritePin(nSPI2_NSS_Z_GPIO_Port, nSPI2_NSS_Z_Pin, GPIO_PIN_SET);
            break;
        case MOTOR_X:
            HAL_GPIO_WritePin(nSPI2_NSS_X_GPIO_Port, nSPI2_NSS_X_Pin, GPIO_PIN_SET);
            break;
        case MOTOR_L:
            HAL_GPIO_WritePin(nSPI2_NSS_L_GPIO_Port, nSPI2_NSS_L_Pin, GPIO_PIN_SET);
            break;
    }
}


static void spi2_nss_init(void) {
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_LOW;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Initialize the NSS pins for the motor Z
    gpio.Pin = nSPI2_NSS_Z_Pin;
    HAL_GPIO_Init(nSPI2_NSS_Z_GPIO_Port, &gpio);

    // Initialize the NSS pins for the motor X
    gpio.Pin = nSPI2_NSS_X_Pin;
    HAL_GPIO_Init(nSPI2_NSS_X_GPIO_Port, &gpio);

    // Initialize the NSS pins for the motor L
    gpio.Pin = nSPI2_NSS_L_Pin;
    HAL_GPIO_Init(nSPI2_NSS_L_GPIO_Port, &gpio);

    // Make sure all pins are disabled at boot
    disable_spi_nss();
}


void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(hspi->Instance==SPI2)
    {
        /* Peripheral clock enable */
        __HAL_RCC_SPI2_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = SPI2_SCK_Pin | SPI2_CIPO_Pin | SPI2_COPI_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(SPI2_Port, &GPIO_InitStruct);

        /* SPI2 DMA Init */
        /* SPI2_RX Init */
        hdma_spi2_rx.Instance = DMA1_Channel1;
        hdma_spi2_rx.Init.Request = DMA_REQUEST_SPI2_RX;
        hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_rx.Init.Mode = DMA_NORMAL;
        hdma_spi2_rx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK)
        {
            // TODO: implement error handling
            return;
        }

        __HAL_LINKDMA(hspi,hdmarx,hdma_spi2_rx);

        /* SPI2_TX Init */
        hdma_spi2_tx.Instance = DMA1_Channel2;
        hdma_spi2_tx.Init.Request = DMA_REQUEST_SPI2_TX;
        hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_tx.Init.Mode = DMA_NORMAL;
        hdma_spi2_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK)
        {
            // TODO: implement error handling
            return;
        }

        __HAL_LINKDMA(hspi,hdmatx,hdma_spi2_tx);

        /* SPI2 interrupt Init */
        HAL_NVIC_SetPriority(SPI2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(SPI2_IRQn);

        /* DMA controller clock enable */
        __HAL_RCC_DMAMUX1_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* DMA interrupt init */
        /* DMA1_Channel1_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        /* DMA1_Channel2_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);


    }
}


void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{
    if(hspi->Instance==SPI2)
    {
        /* Peripheral clock disable */
        __HAL_RCC_SPI2_CLK_DISABLE();

        HAL_GPIO_DeInit(SPI2_Port, SPI2_SCK_Pin | SPI2_CIPO_Pin | SPI2_COPI_Pin);

        /* SPI2 DMA DeInit */
        HAL_DMA_DeInit(hspi->hdmarx);
        HAL_DMA_DeInit(hspi->hdmatx);

        /* SPI2 interrupt DeInit */
        HAL_NVIC_DisableIRQ(SPI2_IRQn);
    }
}


/** Public Functions -------------------------------------------------------- */

void spi_hardware_init(void) {
    if (!_spi.initialized) {

        /* SPI2 parameter configuration*/
        _spi.handle.Instance = SPI2;
        _spi.handle.Init.Mode = SPI_MODE_MASTER;
        _spi.handle.Init.Direction = SPI_DIRECTION_2LINES;
        _spi.handle.Init.DataSize = SPI_DATASIZE_8BIT;
        _spi.handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
        _spi.handle.Init.CLKPhase = SPI_PHASE_2EDGE;
        _spi.handle.Init.NSS = SPI_NSS_SOFT;
        _spi.handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
        _spi.handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
        _spi.handle.Init.TIMode = SPI_TIMODE_DISABLE;
        _spi.handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        _spi.handle.Init.CRCPolynomial = 7;
        _spi.handle.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
        _spi.handle.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        if (HAL_SPI_Init(&_spi.handle) != HAL_OK)
        {
            // TODO: implement error handling
            return;
        }

        // configure nss pins
        spi2_nss_init();

        _spi.initialized = true;
    }

}

/**
  * @brief This function handles DMA1 channel1 global interrupt.
 */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi2_rx);
}

/**
  * @brief This function handles DMA1 channel2 global interrupt.
 */
void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi2_tx);
}

/**
  * @brief This function handles SPI2 global interrupt.
 */
void SPI2_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&_spi.handle);
}

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
  * @brief TxRx Transfer completed callback.
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    spi_interrupt_service();
}

/**
  * @brief Spi Transfer Error callback.
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    spi_interrupt_service();
}

bool spi_dma_transmit_receive(
    MotorID motor_id, uint8_t *txData, uint8_t *rxData, uint16_t size
) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
//    HAL_StatusTypeDef ret;
    uint32_t notification_val = 0;

    if (!_spi.initialized || (_spi.task_to_notify != NULL) || (size > MOTOR_MAX_SPI_LEN)) {
        return false;
    }
    // enable one of the motor driver for SPI communication
    enable_spi_nss(motor_id);
    _spi.task_to_notify = xTaskGetCurrentTaskHandle();
    if (HAL_SPI_TransmitReceive_DMA(&_spi.handle, txData, rxData, size) != HAL_OK) {
        // Transmission error
        _spi.task_to_notify = NULL;
        return false;
    }
    notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    disable_spi_nss();
    // If the task was preempted by the error handler rather than the
    // TxRx complete callback, the remaining count should be greater
    // than 0.
    if((notification_val != 1) || (_spi.handle.RxXferCount > 0)) {
        _spi.task_to_notify = NULL;
        return true;
    }
    return true;
}

