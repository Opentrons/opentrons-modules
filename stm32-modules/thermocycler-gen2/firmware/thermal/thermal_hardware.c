/**
 * @file thermal_hardware.c
 * @details
 * The thermal_hardware.c file contains any HAL-linked control of Thermal System
 * hardware. This is shared between two main tasks, the Lid Heater task and the
 * Thermal Plate task. Each one uses the I2C bus to read from the ADC chips
 * on the board, and the interrupt lines are used to signal the end of an ADC
 * conversion.
 * 
 * The I2C functions use FreeRTOS synchronization objects to provide thread
 * safety and nonblocking behavior.
 *  - A binary semaphore is used to block all access to the I2C bus, so only
 * one thread at a time may access the bus. 
 *  - A task notification system is used to signal the end of interrupt-based
 * I2C transactions. Once a transaction begins, the thread which initiated the
 * transaction sleeps until the end-of-transmission interrupt signals that
 * the data is either done sending or done being received.
 * 
 * Note that the thread safety only applies to individual reads to the I2C bus.
 * Transactions which constitute multiple reads in a row (e.g. reading from an
 * ADC) may require further semaphore use on a higher level.
 */

#include "firmware/thermal_hardware.h"

#include <stdbool.h>
#include <stdatomic.h>

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_i2c.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

// Local includes
#include "firmware/thermal_fan_hardware.h"
#include "firmware/thermal_heater_hardware.h"
#include "firmware/thermal_peltier_hardware.h"

/** Private definitions */

#define I2C_INSTANCE (I2C2)
/* Driven by PCLK1 to Fast Mode - just shy of 400kHz */
#define I2C_TIMING   (0x80500D1D)
/** Max buffer: 2 data bytes*/
#define I2C_BUF_MAX (2)
/** Size of register address: 1 byte.*/
#define REGISTER_ADDR_LEN (1)
/** NVIC priority of ADC interrupts.
 * On the higher end (low-priority) because timing
 * is not critical compared to other interrupts.
 */
#define ADC_READY_ITR_PRIO (4)

/** EEPROM write protect pin */
#define EEPROM_WRITE_PROTECT_PIN  (GPIO_PIN_10)
/** EEPROM write protect port */
#define EEPROM_WRITE_PROTECT_PORT (GPIOC)

/** Local variables */

static _Atomic TaskHandle_t _i2c_task_to_notify = NULL;
static atomic_flag _initialization_started = ATOMIC_FLAG_INIT;
static bool _initialization_done = false;

/** When an ADC READY pin is triggered, which task to notify.*/
static TaskHandle_t _gpio_task_to_notify[ADC_ITR_NUM] = {NULL};
/** Mapping from ITR enum to actual pin numbers.*/
static const uint16_t _adc_itr_gpio[ADC_ITR_NUM] = {
    GPIO_PIN_9,
    GPIO_PIN_10,
};

/** Synchronization primitive for I2C*/
SemaphoreHandle_t _i2c_semaphore = NULL;
/** Static buffer for the semaphore*/
StaticSemaphore_t _i2c_semaphore_buffer;

/** There's only one I2C handle for the device*/
I2C_HandleTypeDef _i2c_handle;
/** Buffers are shared for writing/reading.*/
uint8_t _i2c_buffer[I2C_BUF_MAX];

/** Local functions */

static void thermal_gpio_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /*Configure GPIO pins : ADC_1_ALERT_Pin ADC_2_ALERT_Pin */
    GPIO_InitStruct.Pin = _adc_itr_gpio[ADC1_ITR] |
                          _adc_itr_gpio[ADC2_ITR];
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin: EEPROM Write Protect */
    GPIO_InitStruct.Pin = EEPROM_WRITE_PROTECT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EEPROM_WRITE_PROTECT_PORT, &GPIO_InitStruct);
    // Initialize to protected
    thermal_eeprom_set_write_protect(true);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, ADC_READY_ITR_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, ADC_READY_ITR_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void thermal_i2c_init(void) {
    HAL_StatusTypeDef hal_ret;
    // Initialize I2C 
    _i2c_handle.Instance = I2C_INSTANCE;
    _i2c_handle.Init.Timing = I2C_TIMING;
    _i2c_handle.Init.OwnAddress1 = 0;
    _i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    _i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    _i2c_handle.Init.OwnAddress2 = 0;
    _i2c_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    _i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    _i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    hal_ret = HAL_I2C_Init(&_i2c_handle);
    configASSERT(hal_ret == HAL_OK);
    /** Configure Analogue filter */
    hal_ret = HAL_I2CEx_ConfigAnalogFilter(&_i2c_handle, I2C_ANALOGFILTER_ENABLE);
    configASSERT(hal_ret == HAL_OK);
    /** Configure Digital filter */
    hal_ret = HAL_I2CEx_ConfigDigitalFilter(&_i2c_handle, 0);
    configASSERT(hal_ret == HAL_OK);
    /** I2C Fast mode Plus enable */
    __HAL_SYSCFG_FASTMODEPLUS_ENABLE(I2C_FASTMODEPLUS_I2C2);
}

/** Public functions */

void thermal_hardware_setup(void) {
    bool initialized = atomic_flag_test_and_set(&_initialization_started);
    if(initialized == true) {
        // Wait until _initialization_done is finished by the other thread
        thermal_hardware_wait_for_init();
        return;
    }
    else {
        // Initialize semaphore
        _i2c_semaphore = xSemaphoreCreateMutexStatic(&_i2c_semaphore_buffer);
        configASSERT(_i2c_semaphore != NULL);
        thermal_gpio_init();
        thermal_i2c_init();
        thermal_peltier_initialize();
        thermal_fan_initialize();
        thermal_heater_initialize();

        _initialization_done = true;
    }
}

void thermal_hardware_wait_for_init(void)
{
    while(_initialization_done != true) {
        // Give up execution
        taskYIELD();
    }
}

bool thermal_i2c_write_16(uint16_t addr, uint8_t reg, uint16_t val) {
    const uint16_t bytes_to_send = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    sem_ret = xSemaphoreTake(_i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    _i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    // Prepare buffer & send it
    _i2c_buffer[0] = (val >> 8) & 0xFF;
    _i2c_buffer[1] = (val & 0xFF);
    hal_ret = HAL_I2C_Mem_Write_IT(&_i2c_handle, addr, (uint16_t)reg,
                                   REGISTER_ADDR_LEN, _i2c_buffer, 
                                   bytes_to_send);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(_i2c_semaphore);

    return (notification_val == 1) && (hal_ret == HAL_OK);
}

bool thermal_i2c_read_16(uint16_t addr, uint8_t reg, uint16_t *val) {
    const uint16_t bytes_to_read = 2;
    const TickType_t max_block_time = pdMS_TO_TICKS(100);
    BaseType_t sem_ret;
    uint32_t notification_val = 0;
    HAL_StatusTypeDef hal_ret;

    sem_ret = xSemaphoreTake(_i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }
    
    // Set up notification info
    if(_i2c_task_to_notify != NULL) {
        xSemaphoreGive(_i2c_semaphore);
        return false;
    }
    _i2c_task_to_notify = xTaskGetCurrentTaskHandle();
    
    hal_ret = HAL_I2C_Mem_Read_IT(&_i2c_handle, addr, (uint16_t)reg,
                                  REGISTER_ADDR_LEN, _i2c_buffer,
                                  bytes_to_read);
    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
        if(notification_val == 1) {
            // Only write an output value if we succesfully read from the device
            *val = (((uint16_t)_i2c_buffer[0]) << 8) | ((uint16_t)_i2c_buffer[1]);
        }
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(_i2c_semaphore);

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

    sem_ret = xSemaphoreTake(_i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(_i2c_task_to_notify != NULL) {
        xSemaphoreGive(_i2c_semaphore);
        return false;
    }
    _i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Transmit_IT(&_i2c_handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(_i2c_semaphore);

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

    sem_ret = xSemaphoreTake(_i2c_semaphore, portMAX_DELAY);
    if(sem_ret != pdTRUE) {
        return false;
    }

    // Set up notification info
    if(_i2c_task_to_notify != NULL) {
        xSemaphoreGive(_i2c_semaphore);
        return false;
    }
    _i2c_task_to_notify = xTaskGetCurrentTaskHandle();

    hal_ret = HAL_I2C_Master_Receive_IT(&_i2c_handle, addr, data, len);

    if(hal_ret == HAL_OK) {
        // Block on the interrupt for transmit_complete
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);
    }

    // Ignore return, we would not return an error here even if it fails
    (void)xSemaphoreGive(_i2c_semaphore);
    
    return (notification_val == 1) && (hal_ret == HAL_OK);

}

bool thermal_arm_adc_for_read(ADC_ITR_T id) {
    if(_gpio_task_to_notify[id] != NULL) {
        return false;
    }
    _gpio_task_to_notify[id] = xTaskGetCurrentTaskHandle();
    return true;
}

void thermal_adc_ready_callback(ADC_ITR_T id) {
    // Check that the pin is actually set - the interrupt doesn't do this for us,
    // and other pins trigger the same interrupt vector.
    if(__HAL_GPIO_EXTI_GET_IT(_adc_itr_gpio[id]) != 0x00u) {
        __HAL_GPIO_EXTI_CLEAR_IT(_adc_itr_gpio[id]);
        // There's a possibility of getting an interrupt when we don't expect
        // one, so just ignore if there's no armed task.
        if(_gpio_task_to_notify[id] != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR( _gpio_task_to_notify[id], &xHigherPriorityTaskWoken );
            _gpio_task_to_notify[id] = NULL;
            portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        }
    }
}

void thermal_eeprom_set_write_protect(bool protect) {
    HAL_GPIO_WritePin(
        EEPROM_WRITE_PROTECT_PORT, 
        EEPROM_WRITE_PROTECT_PIN,
        (protect) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** Overwritten HAL functions */

// Interrupt handling is the same for every type of transmission
static void handle_i2c_callback() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if( _i2c_task_to_notify == NULL ) {
        return;
    }
    vTaskNotifyGiveFromISR( _i2c_task_to_notify, &xHigherPriorityTaskWoken );
    _i2c_task_to_notify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

}

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
  /* USER CODE BEGIN I2C2_EV_IRQn 0 */

  /* USER CODE END I2C2_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&_i2c_handle);
  /* USER CODE BEGIN I2C2_EV_IRQn 1 */

  /* USER CODE END I2C2_EV_IRQn 1 */
}

void I2C2_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_ER_IRQn 0 */

  /* USER CODE END I2C2_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&_i2c_handle);
  /* USER CODE BEGIN I2C2_ER_IRQn 1 */

  /* USER CODE END I2C2_ER_IRQn 1 */
}
