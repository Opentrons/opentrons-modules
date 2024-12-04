#include "firmware/i2c_comms.hpp"
#include "firmware/tof_sensor_hardware.h"

#include <stdint.h>

#include "systemwide.h"

using namespace i2c::hardware;

auto I2C::set_handle(HAL_I2C_HANDLE i2c_handle) -> void {
    this->handle = i2c_handle;
    i2c_register_handle(this->handle);
}

auto I2C::enable_tof_sensor(TOFSensorID sensor_id, bool enable) -> void {
    enable_tof_sensor_write(sensor_id, enable);
}

auto I2C::i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data, uint16_t size) -> RxTxReturn {
    MessageT resp{0};
    auto ret = hal_i2c_write(handle, dev_addr, reg, data, size, TIMEOUT);
    return RxTxReturn(ret, resp);
}

auto I2C::i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size) -> RxTxReturn {
    MessageT resp{0};
    auto ret = hal_i2c_read(handle, dev_addr, reg, resp.data(), size, TIMEOUT); 
    return RxTxReturn(ret, resp);
}

