#pragma once

#include <array>
#include <cstdint>

#include "i2c_comms.hpp"

namespace ui_policy {
using namespace i2c::hardware;

class UIPolicy {
  public:
    UIPolicy(I2C *i2c) : i2c_comms{i2c} {}
    auto set_heartbeat_led(bool value) -> void;
    template <size_t Len>
    auto i2c_write(uint8_t device_address, uint8_t register_address,
                   std::array<uint8_t, Len> &data) -> bool {
        auto ret = i2c_comms->i2c_write(device_address, register_address,
                                        data.data(), Len);
        return ret == 0;
    }
    auto i2c_wr(uint8_t device_address, uint8_t register_address, uint8_t *data,
                uint16_t size) -> bool {
        auto ret =
            i2c_comms->i2c_write(device_address, register_address, data, size);
        return ret == 0;
    }
    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t size)
        -> bool {
        auto ret = i2c_comms->i2c_read(dev_addr, reg, data, size);
        return ret == 0;
    }
    auto i2c_master_write(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> bool {
        auto ret = i2c_comms->i2c_master_write(dev_addr, data, size);
        return ret == 0;
    }
    auto i2c_master_read(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> bool {
        auto ret = i2c_comms->i2c_master_read(dev_addr, data, size);
        return ret == 0;
    }
    auto static sleep_ms(uint32_t ms) -> void;

  public:
    I2C *i2c_comms;
};
}  // namespace ui_policy
