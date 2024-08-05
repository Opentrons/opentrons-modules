#include "firmware/motor_policy.hpp"

#include "firmware/motor_hardware.h"

using namespace motor_policy;

auto MotorPolicy::enable_motor(MotorID motor_id) -> void {
    enable_motor(motor_id);
}

auto MotorPolicy::disable_motor(MotorID motor_id) -> void {
    disable_motor(motor_id);
}

auto MotorPolicy::step(MotorID motor_id) -> void { step(motor_id); }
