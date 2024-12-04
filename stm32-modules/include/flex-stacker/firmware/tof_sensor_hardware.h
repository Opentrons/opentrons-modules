#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef void *HAL_I2C_HANDLE;

typedef struct HandlerStruct {
    HAL_I2C_HANDLE i2c2;
    HAL_I2C_HANDLE i2c3;
} I2CHandlerStruct;


void i2c_hardware_init(I2CHandlerStruct *i2c_handles);
bool i2c_register_handle(HAL_I2C_HANDLE handle);
uint8_t hal_i2c_write(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg,
                      uint8_t *data, uint16_t size, uint32_t timeout);
uint8_t hal_i2c_read(HAL_I2C_HANDLE handle, uint16_t DevAddress, uint16_t reg,
                     uint8_t *data, uint16_t size, uint32_t timeout);

void enable_eeprom_write(bool enable);
void enable_tof_sensor_write(TOFSensorID sensor_id, bool enable);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
