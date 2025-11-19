#pragma once

#include <atomic>
#include <cstdint>

#include "i2c_comms.hpp"
#include "systemwide.h"

namespace pressure_policy {
using namespace i2c::hardware;

class PressurePolicy {
  public:
    PressurePolicy(TaskHandle hw_handle, std::atomic<bool> *t_sync, I2C *i2c1,
                   I2C *i2c2, I2C *i2c3)
        : hardware_handle(hw_handle),
          t_resync_needed(t_sync),
          i2c1_comms{i2c1},
          i2c2_comms{i2c2},
          i2c3_comms{i2c3} {}

    auto get_i2c_comms(PressureSensorID sensor_id) -> I2C * {
        switch (sensor_id) {
            case PressureSensorID::ABS_PRESSURE_A:
                return i2c1_comms;
            case PressureSensorID::ABS_PRESSURE_B:
                return i2c3_comms;
            case PressureSensorID::ATM_PRESSURE:
                return i2c2_comms;
            default:
                return nullptr;
        }
    }

    auto static conversion_ended(PressureSensorID sensor_id) -> bool;
    auto static sensor_reset(PressureSensorID sensor_id) -> void;
    auto static sleep_ms(uint32_t ms) -> void;
    [[nodiscard]] auto get_time_ms() const -> uint32_t;
    auto enable_continous_pressure(bool enable) -> void;

  private:
    TaskHandle hardware_handle;
    std::atomic<bool> *t_resync_needed;

    I2C *i2c1_comms;
    I2C *i2c2_comms;
    I2C *i2c3_comms;
};
}  // namespace pressure_policy
