#include "firmware/thermistor_hardware.h"

#include <stdbool.h>
#include <stdatomic.h>

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_i2c.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"


/** Private definitions */

#define ADC_ALERT_PIN  (GPIO_PIN_11)
#define ADC_ALERT_PORT (GPIOB)

#define I2C_INSTANCE (I2C2)
/* Driven by PCLK1 to Fast Mode - just shy of 400kHz */
#define I2C_TIMING   (0x80500D1D)
/** Max buffer: 2 data bytes*/
#define I2C_BUF_MAX (2)
/** Size of register address: 1 byte.*/
#define REGISTER_ADDR_LEN (1)

#define SDA_PIN (GPIO_PIN_8)
#define SDA_PORT (GPIOA)
#define SCL_PIN (GPIO_PIN_9)
#define SCL_PORT (GPIOA)
#define ENABLE_I2C_GPIO_CLK() __HAL_RCC_GPIOA_CLK_ENABLE()

/** Private typedef */

struct ThermistorHardware {
    I2C_HandleTypeDef i2c_handle;
    // Used to signal the end of an I2C transaction
    _Atomic TaskHandle_t i2c_task_to_notify;
    // Used to signal the ADC Alert pin interrupt
    _Atomic TaskHandle_t gpio_task_to_notify;
    SemaphoreHandle_t i2c_semaphore;
    StaticSemaphore_t i2c_semaphore_data;
    uint8_t i2c_buffer[I2C_BUF_MAX];
    _Atomic bool initialized;
    _Atomic bool initialization_started;
};

/** Static variables */

static struct ThermistorHardware hardware = {
    .i2c_handle = {0},
    .i2c_task_to_notify = NULL,
    .gpio_task_to_notify = NULL,
    .i2c_semaphore = NULL,
    .i2c_semaphore_data = {},
    .i2c_buffer = {0},
    .initialized = false,
    .initialization_started = false,
};

/** Static function declaration */
static void handle_i2c_callback();

/** Public functions */


void thermistor_hardware_init() {
    GPIO_InitTypeDef gpio_init = {0};

    // Enforce that only one task may initialize the I2C
    if(atomic_exchange(&hardware.initialization_started, true) == false) {
        hardware.i2c_semaphore = 
            xSemaphoreCreateMutexStatic(&hardware.i2c_semaphore_data);

        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* Configure the ADC Alert pin*/
        gpio_init.Pin = ADC_ALERT_PIN;
        gpio_init.Mode = GPIO_MODE_IT_FALLING;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(ADC_ALERT_PORT, &gpio_init);

        HAL_StatusTypeDef hal_ret;
        // Initialize I2C 
        hardware.i2c_handle.State = HAL_I2C_STATE_RESET;
        hardware.i2c_handle.Instance = I2C_INSTANCE;
        hardware.i2c_handle.Init.Timing = I2C_TIMING;
        hardware.i2c_handle.Init.OwnAddress1 = 0;
        hardware.i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
        hardware.i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
        hardware.i2c_handle.Init.OwnAddress2 = 0;
        hardware.i2c_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
        hardware.i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
        hardware.i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
        hal_ret = HAL_I2C_Init(&hardware.i2c_handle);
        configASSERT(hal_ret == HAL_OK);
        /** Configure Analogue filter */
        hal_ret = HAL_I2CEx_ConfigAnalogFilter(&hardware.i2c_handle, I2C_ANALOGFILTER_ENABLE);
        configASSERT(hal_ret == HAL_OK);
        /** Configure Digital filter */
        hal_ret = HAL_I2CEx_ConfigDigitalFilter(&hardware.i2c_handle, 0);
        configASSERT(hal_ret == HAL_OK);

        /** Configure interrupt for ADC Alert pin */
        HAL_NVIC_SetPriority(EXTI15_10_IRQn, 4, 0);
        HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

        hardware.initialized = true;
    } else {
        // Spin until the hardware is initialized
        while(!hardware.initialized) {
            taskYIELD();
        }
    }
}

bool thermal_i2c_write_16(uint16_t addr, uint8_t reg, uint16_t val) {
    const uint16_t bytes_to_send = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    if(!hardware.initialized) {
        return false;
    }

    sem_ret = xSemaphoreTake(hardware.i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    hardware.i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    // Prepare buffer & send it
    hardware.i2c_buffer[0] = (val >> 8) & 0xFF;
    hardware.i2c_buffer[1] = (val & 0xFF);
    hal_ret = HAL_I2C_Mem_Write_IT(&hardware.i2c_handle, addr, (uint16_t)reg,
                                   REGISTER_ADDR_LEN, hardware.i2c_buffer, 
                                   bytes_to_send);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(hardware.i2c_semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool thermal_i2c_read_16(uint16_t addr, uint8_t reg, uint16_t *val) {
    const uint16_t bytes_to_read = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    if(!hardware.initialized) {
        return false;
    }

    sem_ret = xSemaphoreTake(hardware.i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }
    
    // Set up notification info
    if(hardware.i2c_task_to_notify != NULL) {
        xSemaphoreGive(hardware.i2c_semaphore);
        return false;
    }
    hardware.i2c_task_to_notify = xTaskGetCurrentTaskHandle();
    
    hal_ret = HAL_I2C_Mem_Read_IT(&hardware.i2c_handle, addr, (uint16_t)reg,
                                  REGISTER_ADDR_LEN, hardware.i2c_buffer,
                                  bytes_to_read);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
        if(notification_val == 1) {
            // Only write an output value if we succesfully read from the device
            *val = (((uint16_t)hardware.i2c_buffer[0]) << 8) | ((uint16_t)hardware.i2c_buffer[1]);
        }
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(hardware.i2c_semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool thermal_i2c_write_data(uint16_t addr, uint8_t *data, uint16_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef hal_ret;
    uint32_t notification_val = 0;
    BaseType_t sem_ret;

    if(data == NULL) {
        return false;
    }

    sem_ret = xSemaphoreTake(hardware.i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(hardware.i2c_task_to_notify != NULL) {
        xSemaphoreGive(hardware.i2c_semaphore);
        return false;
    }
    hardware.i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Transmit_IT(&hardware.i2c_handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(hardware.i2c_semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool thermal_i2c_read_data(uint16_t addr, uint8_t *data, uint16_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef hal_ret;
    uint32_t notification_val = 0;
    BaseType_t sem_ret;

    if(data == NULL) {
        return false;
    }

    sem_ret = xSemaphoreTake(hardware.i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(hardware.i2c_task_to_notify != NULL) {
        xSemaphoreGive(hardware.i2c_semaphore);
        return false;
    }
    hardware.i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Receive_IT(&hardware.i2c_handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(hardware.i2c_semaphore);
    
    return (notification_val == 1) && (hal_ret == HAL_OK);

}

bool thermal_arm_adc_for_read() {
    hardware.gpio_task_to_notify = xTaskGetCurrentTaskHandle();
    return true;
}

void thermal_adc_ready_callback() {
    // Check that the pin is actually set - the interrupt doesn't do this for us,
    // and other pins trigger the same interrupt vector.
    if(__HAL_GPIO_EXTI_GET_IT(ADC_ALERT_PIN) != 0x00u) {
        __HAL_GPIO_EXTI_CLEAR_IT(ADC_ALERT_PIN);
        // There's a possibility of getting an interrupt when we don't expect
        // one, so just ignore if there's no armed task.
        if(hardware.gpio_task_to_notify != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR( hardware.gpio_task_to_notify, &xHigherPriorityTaskWoken );
            hardware.gpio_task_to_notify = NULL;
            portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        }
    }
}

// Interrupt handling is the same for every type of transmission
static void handle_i2c_callback() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if( hardware.i2c_task_to_notify == NULL ) {
        return;
    }
    vTaskNotifyGiveFromISR(hardware.i2c_task_to_notify, 
                           &xHigherPriorityTaskWoken);
    hardware.i2c_task_to_notify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
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
    if(hi2c->Instance==I2C2)
    {
        ENABLE_I2C_GPIO_CLK();
        GPIO_InitStruct.Pin = SCL_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(SCL_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = SDA_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C2_CLK_ENABLE();
        /* I2C2 interrupt Init */
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
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
        PC4     ------> I2C2_SCL
        PA8     ------> I2C2_SDA
        */
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4);

        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8);

        /* I2C2 interrupt DeInit */
        HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
    }
}

/** Overwritten HAL functions */

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_handle){
    handle_i2c_callback();
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_handle){
    handle_i2c_callback();
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    handle_i2c_callback();
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    handle_i2c_callback();
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_handle)
{
    handle_i2c_callback();
}

/** Interrupt handlers */

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hardware.i2c_handle);
}

void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hardware.i2c_handle);
}
