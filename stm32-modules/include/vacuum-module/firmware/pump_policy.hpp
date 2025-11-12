#pragma once

#include <cstdint>

#include "firmware/pump_hardware.h"
#include "systemwide.h"

namespace pump_policy {

class PumpPolicy {
  public:
    auto start_pump_motor() -> bool;
    auto stop_pump_motor() -> bool;
    auto set_pump_duty_cycle(uint16_t duty) -> void;
    auto get_pump_duty_cycle() -> uint16_t;
    auto enable_pump_tach(bool enable) -> bool;
    auto get_pump_rpm() -> float;
    auto sleep_ms(uint32_t ms) -> void;
};
}  // namespace pump_policy
