#pragma once

#include <cstdint>

#include "systemwide.h"

namespace motor_policy {

class MotorPolicy {
  public:
    auto enable_motor(MotorID motor_id) -> bool;
    auto start_motor_timer(MotorID motor_id) -> void;
    auto disable_motor(MotorID motor_id) -> bool;
    auto stop_motor(MotorID motor_id) -> bool;
    auto step(MotorID motor_id) -> void;
    auto set_direction(MotorID motor_id, bool direction) -> void;
    auto set_limit_switch_irq(MotorID motor_id, bool direction, bool enable)
        -> void;
    auto check_limit_switch(MotorID motor_id, bool direction) -> bool;
    auto set_diag0_irq(bool enable) -> void;
    auto check_platform_sensor(bool direction) -> bool;
    auto check_estop() -> bool;
    auto check_diag0() -> bool;
    auto is_diag0_pin(uint16_t pin) -> bool;
    auto is_estop_pin(uint16_t pin) -> bool;
    auto sleep_ms(uint32_t ms) -> void;
};
}  // namespace motor_policy
