#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef void *HAL_I2C_HANDLE;

/**
 * @brief Before using an I2C struct, it should be "registered" so that
 * the callbacks can be associated with the HAL I2C handles.
 */
bool i2c_register_handle(HAL_I2C_HANDLE handle);

/**
 * Wrapper around HAL_I2C_Master_Transmit
 */
uint8_t hal_i2c_write(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg,
                      uint8_t *data, uint16_t size, uint32_t timeout);

/**
 * Wrapper around HAL_I2C_Master_Receive
 */
uint8_t hal_i2c_read(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg,
                     uint8_t *data, uint16_t size, uint32_t timeout);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
