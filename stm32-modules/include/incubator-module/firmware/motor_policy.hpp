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
    auto check_home_sensor(MotorID motor_id) -> bool;
    auto sleep_ms(uint32_t ms) -> void;
};
}  // namespace motor_policy
