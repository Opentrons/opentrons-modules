#pragma once

#include <cstdint>

#include "firmware/pump_hardware.h"
#include "systemwide.h"

namespace pump_policy {

class PumpPolicy {
  public:
    PumpPolicy(TaskHandle hw_handle) : hardware_handle(hw_handle) {}

    auto start_pump_motor() -> bool;
    auto stop_pump_motor() -> bool;
    auto set_pump_duty_cycle(uint16_t duty) -> void;
    auto get_pump_duty_cycle() -> uint16_t;
    auto enable_pump_tach(bool enable) -> bool;
    auto get_pump_rpm() -> float;
    auto sleep_ms(uint32_t ms) -> void;
    [[nodiscard]] auto get_time_ms() const -> uint32_t;
    auto enable_pump_control(bool enable) -> void;

  private:
    TaskHandle hardware_handle;
};
}  // namespace pump_policy
