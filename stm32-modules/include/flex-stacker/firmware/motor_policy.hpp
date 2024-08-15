#pragma once

#include <cstdint>

#include "systemwide.h"

namespace motor_policy {

class MotorPolicy {
  public:
    auto enable_motor(MotorID motor_id) -> bool;
    auto disable_motor(MotorID motor_id) -> bool;
    auto step(MotorID motor_id) -> void;
};

}  // namespace motor_policy
