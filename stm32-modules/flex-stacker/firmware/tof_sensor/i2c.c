#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_it.h"
#include "FreeRTOS.h"

#include "firmware/i2c.h"
#include "task.h"

#define MAX_I2C_HANDLES (3)

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


/**
 * Wrapper around HAL_I2C_Master_Transmit
 */
bool hal_i2c_master_transmit(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint8_t *data, uint16_t size, uint32_t timeout)
{
    uint32_t notification_val = 0;
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notification_handle = lookup_handle(i2c_handle);

    if(notification_handle == NULL) {
        return false;
    }

    uint32_t tickstart = HAL_GetTick();
    HAL_StatusTypeDef tx_result = HAL_OK;
    do {
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        tx_result = HAL_I2C_Master_Transmit_IT(i2c_handle,
                            DevAddress, data, size);
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
    return tx_result == HAL_OK;
}

/**
 * Wrapper around HAL_I2C_Master_Receive
 */
bool hal_i2c_master_receive(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint8_t *data, uint16_t size, uint32_t timeout){
    uint32_t notification_val = 0;
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notification_handle = lookup_handle(i2c_handle);

    if(notification_handle == NULL) {
        return false;
    }

    uint32_t tickstart = HAL_GetTick();
    HAL_StatusTypeDef rx_result = HAL_OK;
    do {
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        rx_result = HAL_I2C_Master_Receive_IT(i2c_handle, DevAddress, data, size);
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
    return rx_result == HAL_OK;
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

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_handle)
{
    handle_i2c_callback(i2c_handle, true);
}
