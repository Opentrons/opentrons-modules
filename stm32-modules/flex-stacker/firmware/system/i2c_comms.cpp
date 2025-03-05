#include <cstdint>

#include "systemwide.h"
#include "firmware/i2c_hardware.h"
#include "firmware/hardware_iface.hpp"
#include "firmware/i2c_comms.hpp"

using namespace i2c::hardware;

auto I2C::set_handle(HAL_I2C_HANDLE i2c_handle, I2C_BUS i2c_bus) -> void {
    this->bus = i2c_bus;
    i2c_register_handle(i2c_handle, i2c_bus);
}

auto I2C::i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                    uint16_t size) -> RxTxReturn {
    auto ret = hal_i2c_write(bus, dev_addr, reg, data, size);
    return RxTxReturn(ret);
}

auto I2C::i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                   uint16_t size) -> RxTxReturn {
    auto ret = hal_i2c_read(bus, dev_addr, reg, data, size);
    return RxTxReturn(ret);
}
