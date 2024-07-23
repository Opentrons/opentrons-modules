#pragma once

#include <cstdint>

#include "systemwide.h"

namespace motor_policy {

class MotorPolicy {
  public:
    auto enable_motor(MotorID motor_id) -> void;
    auto disable_motor(MotorID motor_id) -> void;
    auto set_motor_speed(MotorID motor_id, double speed) -> bool;
};

}  // namespace motor_policy
