#include "firmware/motor_policy.hpp"

#include "firmware/motor_hardware.h"

using namespace motor_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::enable_motor(MotorID motor_id) -> bool {
    return hw_enable_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::disable_motor(MotorID motor_id) -> bool {
    return hw_disable_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::step(MotorID motor_id) -> void { step_motor(motor_id); }
