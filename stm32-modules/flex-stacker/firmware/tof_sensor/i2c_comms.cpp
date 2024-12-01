#include "firmware/i2c_comms.hpp"

#include <stdint.h>

#include "systemwide.h"

using namespace i2c::hardware;

/*
 * I2C wrapper class.
 *
 * Private:
 * master_transmit - send out a command to I2C
 * master_receive - receive data from the I2C line
 *
 */

auto I2C::transmit_receive(uint16_t dev_address, MessageT& msg,
                           bool read = false) -> RxTxReturn {
    MessageT read_buf{0};
    //central_transmit(msg.data(), msg.size(), dev_address, TIMEOUT);
    //MessageT read_buf{ret};
    //return RxTxReturn(read_buf);
    if (read) {
        //central_receive(read_buf.data(), read_buf.size(), dev_address, TIMEOUT);
        central_receive(read_buf.data(), 8, dev_address, TIMEOUT);
        //MessageT read_buf{ret};
        return RxTxReturn(read_buf);
    }
    return RxTxReturn();
}

auto I2C::central_transmit(uint8_t* data, uint16_t size, uint16_t dev_address,
                           uint32_t timeout) -> uint8_t {
    return hal_i2c_master_transmit(handle, dev_address, data, size, timeout);
}

auto I2C::central_receive(uint8_t* data, uint16_t size, uint16_t dev_address,
                          uint32_t timeout) -> uint8_t {
    return hal_i2c_master_receive(handle, dev_address, data, size, timeout);
}

auto I2C::set_handle(HAL_I2C_HANDLE i2c_handle) -> void {
    handle = i2c_handle;
    i2c_register_handle(handle);
}
