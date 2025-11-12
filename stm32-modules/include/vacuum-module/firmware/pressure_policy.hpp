#pragma once

#include <cstdint>

#include "i2c_comms.hpp"
#include "systemwide.h"

namespace pressure_policy {
using namespace i2c::hardware;

class PressurePolicy {
  public:
    PressurePolicy(I2C *i2c) : i2c_comms{i2c} {}

    template <size_t Len>
    auto i2c_write(uint8_t device_address, uint8_t register_address,
                   std::array<uint8_t, Len> &data) -> bool {
        auto ret = i2c_comms->i2c_write(device_address, register_address,
                                        data.data(), Len);
        return ret == 0;
    }
    auto static sleep_ms(uint32_t ms) -> void;

  private:
    I2C *i2c_comms;
};
}  // namespace pressure_policy
