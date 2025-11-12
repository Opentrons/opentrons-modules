#pragma once

#include <cstdint>

#include "i2c_comms.hpp"
#include "systemwide.h"

namespace pressure_policy {
using namespace i2c::hardware;

class PressurePolicy {
  public:
    PressurePolicy(I2C *i2c1, I2C *i2c2, I2C *i2c3)
        : i2c_comms1{i2c1}, i2c_comms2{i2c2}, i2c_comms3{i2c3} {}

    auto static sleep_ms(uint32_t ms) -> void;

    auto get_i2c_comms(PressureSensorID sensor_id) -> I2C * {
        switch (sensor_id) {
            case PressureSensorID::ABS_PRESSURE_A:
                return i2c_comms1;
            case PressureSensorID::ABS_PRESSURE_B:
                return i2c_comms2;
            case PressureSensorID::ATM_PRESSURE:
                return i2c_comms3;
            default:
                return nullptr;
        }
    }

  private:
    I2C *i2c_comms1;
    I2C *i2c_comms2;
    I2C *i2c_comms3;
};
}  // namespace pressure_policy
