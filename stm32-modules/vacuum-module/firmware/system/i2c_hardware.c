#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

#include "firmware/i2c_hardware.h"

#define MAX_TIMEOUT (pdMS_TO_TICKS(100))
#define MAX_RETRIES (3)
#define MAX_I2C_HANDLES (3)
#define NO_HANDLE_ERROR (255)
#define REGISTER_ADDR_LEN (1)  // 1 byte

static I2C_HandleTypeDef hi2c2;
static I2C_HandleTypeDef hi2c3;

typedef struct {
    I2C_BUS bus;
    I2C_HandleTypeDef *i2c_handle;
    TaskHandle_t task_to_notify;
    bool should_retry;
} NotificationHandle_t;

static NotificationHandle_t _notification_handles[MAX_I2C_HANDLES];

static bool _initialized = false;

HAL_I2C_HANDLE MX_I2C2_Init()
{
    hi2c2.State = HAL_I2C_STATE_RESET;
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10C0ECFF;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Analogue filter
    */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Digital filter
    */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
    {
        Error_Handler();
    }

    /** I2C Fast mode Plus enable */
    __HAL_SYSCFG_FASTMODEPLUS_ENABLE(I2C_FASTMODEPLUS_I2C2);

    return &hi2c2;
}

HAL_I2C_HANDLE MX_I2C3_Init() {
    hi2c3.Instance = I2C3;
    hi2c3.Init.Timing = 0x10C0ECFF;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c3) != HAL_OK) {
        Error_Handler();
    }
    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) !=
        HAL_OK) {
        Error_Handler();
    }
    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK) {
        Error_Handler();
    }

    /** I2C Fast mode Plus enable */
    __HAL_SYSCFG_FASTMODEPLUS_ENABLE(I2C_FASTMODEPLUS_I2C3);

    return &hi2c3;
}

void i2c_hardware_init(I2CHandlerStruct* i2c_handles) {
    HAL_I2C_HANDLE i2c2 = MX_I2C2_Init();
    HAL_I2C_HANDLE i2c3 = MX_I2C3_Init();
    i2c_handles->i2c2 = i2c2;
    i2c_handles->i2c3 = i2c3;
}

/**
 * @brief Get the notification handle based on the I2C Bus.
 * Returns NULL if the handle is not found.
 */
static NotificationHandle_t* lookup_handle(I2C_BUS bus) {
    for(size_t i = 0; i < MAX_I2C_HANDLES; ++i) {
        I2C_BUS notif_bus = _notification_handles[i].bus;
        if(notif_bus != NO_BUS && notif_bus == bus) {
            return &_notification_handles[i];
        }
    }
    return NULL;
}

/**
 * @brief Get the notification handle based on the HAL I2C struct.
 * Returns NULL if the handle is not found.
 */
static NotificationHandle_t* get_handle(I2C_HandleTypeDef *i2c_handle) {
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
            _notification_handles[i].bus = NO_BUS;
        }
        _initialized = true;
    }
}

/**
 * @brief Common handler for all I2C callbacks.
 */
static void handle_i2c_callback(I2C_HandleTypeDef *i2c_handle, bool error) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    NotificationHandle_t *instance = get_handle(i2c_handle);
    if(instance == NULL) return;
    if(instance->task_to_notify == NULL) return;

    vTaskNotifyGiveFromISR(instance->task_to_notify, 
                            &xHigherPriorityTaskWoken);
    instance->task_to_notify = NULL;
    instance->should_retry = error;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

bool i2c_register_handle(HAL_I2C_HANDLE handle, I2C_BUS bus) {
    initialize_notification_handles();

    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)handle;
    NotificationHandle_t *notif_handle = lookup_handle(bus);
    if(notif_handle != NULL) return true;  // Already Registered

    // Now find an empty slot
    notif_handle = get_handle(NULL);
    if(notif_handle == NULL) return false; // No empty slots

    notif_handle->i2c_handle = i2c_handle;
    notif_handle->bus = bus;
    return true;
}

/**
 * Wrapper around HAL_I2C_Mem_Write
 */
uint8_t hal_i2c_write(I2C_BUS bus, uint16_t DevAddress, uint8_t reg, uint8_t *data, uint16_t size) {
    uint32_t notification_val = 0;
    NotificationHandle_t *notification_handle = lookup_handle(bus);
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)notification_handle->i2c_handle;

    // Bus was not registered
    if(notification_handle == NULL) return false;

    uint8_t retries = 0;
    HAL_StatusTypeDef tx_result = HAL_OK;
    do {
        // Setup task handle and send message
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        tx_result = HAL_I2C_Mem_Write_IT(i2c_handle,
                            DevAddress, reg, REGISTER_ADDR_LEN, data, size);

        // Wait for callback and check result
        notification_val = ulTaskNotifyTake(pdTRUE, MAX_TIMEOUT);
        if (tx_result == HAL_OK) break;
        if (notification_handle->should_retry) tx_result = HAL_BUSY;
        if (notification_val != 1) tx_result = HAL_TIMEOUT;

        retries += 1;
    } while(retries < MAX_RETRIES);
    return tx_result;
}

/**
 * Wrapper around HAL_I2C_Mem_Read
 */
uint8_t hal_i2c_read(I2C_BUS bus, uint16_t DevAddress, uint16_t reg, uint8_t *data, uint16_t size) {
    uint32_t notification_val = 0;
    NotificationHandle_t *notification_handle = lookup_handle(bus);
    I2C_HandleTypeDef* i2c_handle = (I2C_HandleTypeDef*)notification_handle->i2c_handle;

    // Bus was not registered
    if(notification_handle == NULL) return NO_HANDLE_ERROR;

    uint8_t retries = 0;
    HAL_StatusTypeDef rx_result = HAL_OK;
    do {
        // Setup task handle and send message
        notification_handle->task_to_notify = xTaskGetCurrentTaskHandle();
        rx_result = HAL_I2C_Mem_Read_IT(i2c_handle,
                            DevAddress, reg, REGISTER_ADDR_LEN, data, size);

        // Wait for callback and check result.
        notification_val = ulTaskNotifyTake(pdTRUE, MAX_TIMEOUT);
        if (rx_result == HAL_OK) break;
        if (notification_handle->should_retry) rx_result = HAL_BUSY;
        if (notification_val != 1) rx_result = HAL_TIMEOUT;

        retries += 1;
    } while (retries < MAX_RETRIES);
    return rx_result;
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_handle) {
    handle_i2c_callback(i2c_handle, false);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_handle) {
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

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c2);
}

void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c2);
}

void I2C3_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c3);
}

void I2C3_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c3);
}
