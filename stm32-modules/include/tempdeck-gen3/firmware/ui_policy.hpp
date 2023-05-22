#pragma once

#include <array>

#include "firmware/i2c_hardware.h"

class UIPolicy {
  public:
    auto set_heartbeat_led(bool value) -> void;

    template <size_t Len>
    auto i2c_write(uint8_t device_address, uint8_t register_address,
                   std::array<uint8_t, Len> &data) -> bool {
        return i2c_hardware_mem_write(I2C_BUS_LED, device_address,
                                      register_address, data.data(), Len);
    }
};
