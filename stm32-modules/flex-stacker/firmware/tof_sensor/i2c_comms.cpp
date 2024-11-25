#include "firmware/i2c_comms.hpp"

using namespace i2c::hardware;

/*
 * I2C wrapper class.
 *
 * Public:
 * master_transmit - send out a command to I2C
 * master_receive - receive data from the I2C line
 *
 *
 */

auto I2C::central_transmit(uint8_t* data, uint16_t size, uint16_t dev_address,
                           uint32_t timeout) -> bool {
    return hal_i2c_master_transmit(handle, dev_address, data, size, timeout);
}

auto I2C::central_receive(uint8_t* data, uint16_t size, uint16_t dev_address,
                          uint32_t timeout) -> bool {
    return hal_i2c_master_receive(handle, dev_address, data, size, timeout);
}

auto I2C::set_handle(HAL_I2C_HANDLE i2c_handle) -> void {
    handle = i2c_handle;
    i2c_register_handle(handle);
}
