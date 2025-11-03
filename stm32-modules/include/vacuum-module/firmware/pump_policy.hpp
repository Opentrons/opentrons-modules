#pragma once

#include <cstdint>

#include "firmware/pump_hardware.h"
#include "systemwide.h"

namespace pump_policy {

class PumpPolicy {
  public:
    auto start_pump_motor(bool start) -> void;
    auto set_pump_duty_cycle(uint16_t duty) -> void;
    auto get_pump_duty_cycle() -> uint16_t;
    auto get_pump_rpm() -> double;
};
}  // namespace pump_policy
