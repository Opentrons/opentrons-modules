#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef void *HAL_I2C_HANDLE;

typedef struct HandlerStruct {
    HAL_I2C_HANDLE i2c2;
} I2CHandlerStruct;

void i2c_setup(I2CHandlerStruct* i2c_handles);

/**
 * @brief Before using an I2C struct, it should be "registered" so that
 * the callbacks can be associated with the HAL I2C handles.
 */
bool i2c_register_handle(HAL_I2C_HANDLE handle);

/**
 * Wrapper around HAL_I2C_Master_Transmit
 */
bool hal_i2c_master_transmit(HAL_I2C_HANDLE handle, uint16_t dev_address,
                             uint8_t *data, uint16_t size, uint32_t timeout);

/**
 * Wrapper around HAL_I2C_Master_Receive
 */
bool hal_i2c_master_receive(HAL_I2C_HANDLE handle, uint16_t dev_address,
                            uint8_t *data, uint16_t size, uint32_t timeout);

/**
 * enable writing to the eeprom.
 */
void enable_eeprom_write();

/**
 * disable writing to the eeprom.
 */
void disable_eeprom_write();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
