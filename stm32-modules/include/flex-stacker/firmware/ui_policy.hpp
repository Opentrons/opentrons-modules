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
        auto [ret, _] = i2c_comms->i2c_write(device_address, register_address,
                                             data.data(), Len);
        return ret == 0;
    }

  private:
    I2C *i2c_comms;
};
};  // namespace ui_policy
