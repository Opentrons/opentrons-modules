#include "firmware/i2c_hardware.h"

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

/** Max buffer: 2 data bytes*/
#define I2C_BUF_MAX (2)

/* Driven by PCLK1 to Fast Mode - just shy of 400kHz */
#define I2C_TIMING   (0x80500D1D)
/** Size of register address: 1 byte.*/
#define REGISTER_ADDR_LEN (1)

#define SDA_PIN (GPIO_PIN_8)
#define SDA_PORT (GPIOA)
#define SCL_PIN (GPIO_PIN_9)
#define SCL_PORT (GPIOA)

/** Private typedef */

typedef struct {
    I2C_TypeDef *instance;
    I2C_HandleTypeDef handle;
    // Used to signal the end of an I2C transaction
    _Atomic TaskHandle_t task_to_notify;
    // Used to maintain lock on i2c access.
    SemaphoreHandle_t semaphore;
    // Backing data for static semaphore
    StaticSemaphore_t semaphore_data;
    // Buffer for I2C data
    uint8_t buffer[I2C_BUF_MAX];
} I2C_Instance;

typedef struct {
    I2C_Instance i2c[I2C_BUS_COUNT];
    _Atomic bool initialized;
    _Atomic bool initialization_started;
} I2C_Hardware;

/** Static variables */

static I2C_Hardware i2c_hardware = {
    .i2c = { 
        {
            .instance = I2C2,
            .handle = {},
            .task_to_notify = NULL,
            .semaphore = NULL,
            .semaphore_data = {},
            .buffer = {0}
        },
        {
            .instance = I2C3,
            .handle = {},
            .task_to_notify = NULL,
            .semaphore = NULL,
            .semaphore_data = {},
            .buffer = {0}
        }
    },
    .initialized = false,
    .initialization_started = false,
};

/** Static function declaration */

/**
 * @brief Initialize one of the I2C busses
 * 
 */
static void i2c_instance_init(I2C_Instance *instance);
static void handle_i2c_callback(I2C_HandleTypeDef *handle);
static inline I2C_Instance*
    i2c_get_struct_from_hal_instance(I2C_TypeDef *instance);

/** Public functions */

void i2c_hardware_init() {
    // Enforce that only one task may initialize the I2C
    if(atomic_exchange(&i2c_hardware.initialization_started, true) == false) {
        for(uint8_t i = 0; i < I2C_BUS_COUNT; ++i) {
            i2c_instance_init(&i2c_hardware.i2c[i]);
        }
        i2c_hardware.initialized = true;
    } else {
        // Spin until the hardware is initialized
        while(!i2c_hardware.initialized) {
            taskYIELD();
        }
    }
}

bool i2c_hardware_write_16(I2C_BUS bus, uint16_t addr, uint8_t reg, uint16_t val) {
    const uint16_t bytes_to_send = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    if(!IS_I2C_BUS(bus)) {
        return false;
    }
    
    I2C_Instance *instance = &i2c_hardware.i2c[bus];
    
    if(!i2c_hardware.initialized) {
        return false;
    }

    sem_ret = xSemaphoreTake(instance->semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    instance->task_to_notify = xTaskGetCurrentTaskHandle();

    // Prepare buffer & send it
    instance->buffer[0] = (val >> 8) & 0xFF;
    instance->buffer[1] = (val & 0xFF);
    hal_ret = HAL_I2C_Mem_Write_IT(&instance->handle, addr, (uint16_t)reg,
                                   REGISTER_ADDR_LEN, instance->buffer, 
                                   bytes_to_send);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(instance->semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool i2c_hardware_read_16(I2C_BUS bus, uint16_t addr, uint8_t reg, uint16_t *val) {
    const uint16_t bytes_to_read = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    if(!IS_I2C_BUS(bus)) {
        return false;
    }

    I2C_Instance *instance = &i2c_hardware.i2c[bus];
    
    if(!i2c_hardware.initialized) {
        return false;
    }

    sem_ret = xSemaphoreTake(instance->semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }
    
    // Set up notification info
    if(instance->task_to_notify != NULL) {
        xSemaphoreGive(instance->semaphore);
        return false;
    }
    instance->task_to_notify = xTaskGetCurrentTaskHandle();
    
    hal_ret = HAL_I2C_Mem_Read_IT(&instance->handle, addr, (uint16_t)reg,
                                  REGISTER_ADDR_LEN, instance->buffer,
                                  bytes_to_read);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
        if(notification_val == 1) {
            // Only write an output value if we succesfully read from the device
            *val = (((uint16_t)instance->buffer[0]) << 8) | ((uint16_t)instance->buffer[1]);
        }
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(instance->semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool i2c_hardware_write_data(I2C_BUS bus, uint16_t addr, uint8_t *data, uint16_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef hal_ret;
    uint32_t notification_val = 0;
    BaseType_t sem_ret;

    if(!IS_I2C_BUS(bus)) {
        return false;
    }

    I2C_Instance *instance = &i2c_hardware.i2c[bus];

    if(!i2c_hardware.initialized) {
        return false;
    }

    if(data == NULL) {
        return false;
    }

    sem_ret = xSemaphoreTake(instance->semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(instance->task_to_notify != NULL) {
        xSemaphoreGive(instance->semaphore);
        return false;
    }
    instance->task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Transmit_IT(&instance->handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(instance->semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool i2c_hardware_read_data(I2C_BUS bus, uint16_t addr, uint8_t *data, uint16_t len) {
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    HAL_StatusTypeDef hal_ret;
    uint32_t notification_val = 0;
    BaseType_t sem_ret;

    if(!IS_I2C_BUS(bus)) {
        return false;
    }

    I2C_Instance *instance = &i2c_hardware.i2c[bus];
    
    if(!i2c_hardware.initialized) {
        return false;
    }
    
    if(data == NULL) {
        return false;
    }

    sem_ret = xSemaphoreTake(instance->semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(instance->task_to_notify != NULL) {
        xSemaphoreGive(instance->semaphore);
        return false;
    }
    instance->task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Receive_IT(&instance->handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(instance->semaphore);
    
    return (notification_val == 1) && (hal_ret == HAL_OK);

}

/*
 * Static functions 
 */

static void i2c_instance_init(I2C_Instance *instance) {
    HAL_StatusTypeDef ret = HAL_ERROR;
    configASSERT(instance != NULL);

    I2C_HandleTypeDef *handle = &instance->handle;

    instance->semaphore =
        xSemaphoreCreateMutexStatic(&instance->semaphore_data);
    configASSERT(instance->semaphore != NULL);

    handle->Instance = instance->instance;
    handle->State = HAL_I2C_STATE_RESET;
    handle->Init.Timing = I2C_TIMING;
    handle->Init.OwnAddress1 = 0;
    handle->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    handle->Init.OwnAddress2 = 0;
    handle->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    handle->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    
    ret = HAL_I2C_Init(handle);
    configASSERT(ret == HAL_OK);
    /** Configure Analogue filter */
    ret = HAL_I2CEx_ConfigAnalogFilter(handle, I2C_ANALOGFILTER_ENABLE);
    configASSERT(ret == HAL_OK);
    /** Configure Digital filter */
    ret = HAL_I2CEx_ConfigDigitalFilter(handle, 0);
    configASSERT(ret == HAL_OK);
}

// Interrupt handling is the same for every type of transmission
static void handle_i2c_callback(I2C_HandleTypeDef *handle) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Need to look up our struct based on the hardware handle...
    I2C_Instance *instance = 
        i2c_get_struct_from_hal_instance(handle->Instance);
    if( (instance == NULL) ||
        instance->task_to_notify == NULL ) {
        return;
    }
    vTaskNotifyGiveFromISR(instance->task_to_notify, 
                           &xHigherPriorityTaskWoken);
    instance->task_to_notify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

// We need to perform lookups from the HAL I2C structs to our
// own structs with higher level data
static inline I2C_Instance*
        i2c_get_struct_from_hal_instance(I2C_TypeDef *instance) {
    if(instance == NULL) {
        return NULL;
    }
    
    for(uint8_t i = 0; i < I2C_BUS_COUNT; ++i) {
        if(i2c_hardware.i2c[i].instance == instance) {
            return &i2c_hardware.i2c[i];
        }
    }
    return NULL;
}

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(hi2c->Instance==I2C2)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
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
    handle_i2c_callback(i2c_handle);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_handle){
    handle_i2c_callback(i2c_handle);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    handle_i2c_callback(hi2c);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    handle_i2c_callback(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_handle)
{
    handle_i2c_callback(i2c_handle);
}

/** Interrupt handlers */

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&i2c_hardware.i2c[I2C_BUS_THERMAL].handle);
}

void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&i2c_hardware.i2c[I2C_BUS_THERMAL].handle);
}
