#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef enum I2C_BUS {
    I2C_BUS_2,
    I2C_BUS_3,
    NO_BUS,
} I2C_BUS;

typedef void *HAL_I2C_HANDLE;

typedef struct HandlerStruct {
    HAL_I2C_HANDLE i2c2;
    HAL_I2C_HANDLE i2c3;
} I2CHandlerStruct;

void i2c_hardware_init(I2CHandlerStruct *i2c_handles);
bool i2c_register_handle(HAL_I2C_HANDLE handle, I2C_BUS bus);
uint8_t hal_i2c_write(I2C_BUS bus, uint16_t DevAddress, uint8_t reg,
                      uint8_t *data, uint16_t size);
uint8_t hal_i2c_read(I2C_BUS bus, uint16_t DevAddress, uint16_t reg,
                     uint8_t *data, uint16_t size);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
