#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_it.h"
#include "FreeRTOS.h"

#include "firmware/i2c.h"
#include "task.h"

#define MAX_I2C_HANDLES (3)
#define NO_HANDLE_ERROR (255)

typedef struct {
    I2C_HandleTypeDef *i2c_handle;
    TaskHandle_t task_to_notify;
    bool should_retry;
} NotificationHandle_t;

static NotificationHandle_t _notification_handles[MAX_I2C_HANDLES];

static bool _initialized = false;

/**
 * @brief Get the notification handle based on the HAL I2C struct.
 * Returns NULL if the handle is not found.
 */
static NotificationHandle_t* lookup_handle(I2C_HandleTypeDef *i2c_handle) {
    for(size_t i = 0; i < MAX_I2C_HANDLES; ++i) {
        if(_notification_handles[i].i2c_handle == i2c_handle) {
            return &_notification_handles[i];
        }
    }
    return NULL;
}

static void initialize_notification_handles() {
    if(!_initialized) {
        for(size_t i = 0; i < MAX_I2C_HANDLES; ++i) {
            _notification_handles[i].i2c_handle = NULL;
            _notification_handles[i].task_to_notify = NULL;
            _notification_handles[i].should_retry = false;
        }
        _initialized = true;
    }
}

/**
 * @brief Common handler for all I2C callbacks.
 */
static void handle_i2c_callback(I2C_HandleTypeDef *i2c_handle, bool error) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    NotificationHandle_t *instance = lookup_handle(i2c_handle);
    if(instance == NULL) {
        return;
    }
    if(instance->task_to_notify == NULL) {
        return;
    }

    vTaskNotifyGiveFromISR(instance->task_to_notify, 
                            &xHigherPriorityTaskWoken);
    instance->task_to_notify = NULL;
    instance->should_retry = error;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

bool i2c_register_handle(HAL_I2C_HANDLE handle) {
    initialize_notification_handles();

    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notif_handle = lookup_handle(i2c_handle);
    if(notif_handle != NULL) {
        // Already registered
        return true;
    }

    // Now find an empty slot
    notif_handle = lookup_handle(NULL);
    if(notif_handle == NULL) {
        // No empty slots
        return false;
    }
    notif_handle->i2c_handle = i2c_handle;
    return true;
}

uint8_t hal_i2c_comms_ready(HAL_I2C_HANDLE handle, uint16_t dev_address, uint8_t tries, uint32_t timeout) {
    HAL_StatusTypeDef status = HAL_OK;
    status = HAL_I2C_IsDeviceReady(handle, dev_address, tries, timeout);
    return (uint8_t)status;
}

// FOR DEBUGGING
#define TMF8822_I2C_ADDRESS  0x41  // Adjust based on your configuration
#define ENABLE_REGISTER      0xE0  // Address of the ENABLE register

uint8_t i2c_tx_complete = 0;
uint8_t i2c_rx_complete = 0;

uint8_t CheckI2CWrite(HAL_I2C_HANDLE handle, uint8_t reg, uint8_t value) {
    HAL_StatusTypeDef status;
    i2c_tx_complete = 0;
    status = HAL_I2C_Mem_Write_IT(handle, TMF8822_I2C_ADDRESS << 1, reg, 1, &value, 1);
    while (i2c_tx_complete != 1) HAL_Delay(1);
    if (status != HAL_OK) return 1;
    return 2;
}

uint8_t CheckI2CRead(HAL_I2C_HANDLE handle, uint8_t reg) {
    uint8_t value = 13;
    HAL_StatusTypeDef status;
    i2c_rx_complete = 0;
    status = HAL_I2C_Mem_Read_IT(handle, TMF8822_I2C_ADDRESS << 1, reg, 2, &value, 1);
    while (i2c_rx_complete != 1) HAL_Delay(1);
    if (status != HAL_OK) return 255;
    return value;
}

uint8_t DebugTMF8820(HAL_I2C_HANDLE handle, uint8_t *data) {
    // Set X Enable pin (PC12)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_Delay(10);
    // Read the ENABLE register
    uint8_t ret = CheckI2CRead(handle, *data);
    return ret;
    if (ret != 0x41) return 1;

    // Read the APPID and MINOR registers
    uint8_t app_id = CheckI2CRead(handle, 0x00);
    uint8_t minor = CheckI2CRead(handle, 0x01);
    // We are in bootloader mode
    if (app_id == 0x80) {
       return minor; 
    }
    return minor;
}


/**
 * Wrapper around HAL_I2C_Master_Transmit
 */
uint8_t hal_i2c_write(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg, uint8_t *data, uint16_t size, uint32_t timeout)
{
    // FOR DEBUGGING
    if(0) {
        uint8_t ret = DebugTMF8820(handle, data); 
        return ret;
    }

    uint32_t notification_val = 0;
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notification_handle = lookup_handle(i2c_handle);

    if(notification_handle == NULL) {
        return NO_HANDLE_ERROR;
    }

    // Make sure the device is online
    HAL_StatusTypeDef dev_status = HAL_OK;
    dev_status = hal_i2c_comms_ready(handle, DevAddress, 3, timeout);
    if (dev_status != HAL_OK) return dev_status;

    uint32_t tickstart = HAL_GetTick();
    HAL_StatusTypeDef tx_result = HAL_OK;
    do {
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        // NOTE: Im setting the register address size to 2, confirm that this works.
        // IT IS CRITICAL TO GET THE reg_size correct, otherwise operation will fail.
        // Do we need to set this dynamically? what about registers with 1 byte?
        tx_result = HAL_I2C_Mem_Write_IT(i2c_handle,
                            DevAddress, reg, 2, data, size);
        notification_val = ulTaskNotifyTake(pdTRUE, timeout); // Wait for callback
        if (notification_handle->should_retry) {
            tx_result = HAL_BUSY;
        }
        if(notification_val != 1) {
            // Interrupt never fired
            tx_result = HAL_TIMEOUT;
        }
        if (tx_result == HAL_OK) {
            break;
        }
    } while ((HAL_GetTick() - tickstart) < timeout);
    return (uint8_t)tx_result;
}

/**
 * Wrapper around HAL_I2C_Master_Receive
 */
uint8_t hal_i2c_read(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg, uint8_t *data, uint16_t size, uint32_t timeout){

    // FOR DEBUGGING
    if(0) {
        uint8_t ret = DebugTMF8820(handle, data); 
        return ret;
    }

    uint32_t notification_val = 0;
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notification_handle = lookup_handle(i2c_handle);

    if(notification_handle == NULL) {
        return NO_HANDLE_ERROR;
    }

    // Make sure the device is online
    HAL_StatusTypeDef dev_status = HAL_OK;
    dev_status = hal_i2c_comms_ready(handle, DevAddress, 3, timeout);
    if (dev_status != HAL_OK) return dev_status;

    uint32_t tickstart = HAL_GetTick();
    HAL_StatusTypeDef rx_result = HAL_OK;
    do {
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        // NOTE: Im setting the register address size to 1, confirm that this works.
        // IT IS CRITICAL TO GET THE reg_size correct, otherwise operation will fail.
        // Do we need to set this dynamically? what about registers with 2 bytes?
        rx_result = HAL_I2C_Mem_Read_IT(i2c_handle,
                            DevAddress, reg, 1, data, size);
        notification_val = ulTaskNotifyTake(pdTRUE, timeout); // Wait for callback
        if (notification_handle->should_retry) {
            rx_result = HAL_BUSY;
        }
        if(notification_val != 1) {
            // Interrupt never fired
            rx_result = HAL_TIMEOUT;
        }
        if (rx_result == HAL_OK) {
            break;
        }
    } while ((HAL_GetTick() - tickstart) < timeout);
    return (uint8_t)rx_result;
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_handle){
    handle_i2c_callback(i2c_handle, false);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_handle){
    handle_i2c_callback(i2c_handle, false);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *i2c_handle) {
    handle_i2c_callback(i2c_handle, false);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *i2c_handle) {
    handle_i2c_callback(i2c_handle, false);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_handle) {
    handle_i2c_callback(i2c_handle, true);
}
