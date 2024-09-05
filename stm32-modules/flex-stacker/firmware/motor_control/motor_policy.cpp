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
auto MotorPolicy::stop_motor(MotorID motor_id) -> bool {
    return hw_stop_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::step(MotorID motor_id) -> void { hw_step_motor(motor_id); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::set_direction(MotorID motor_id, bool direction) -> void {
    hw_set_direction(motor_id, direction);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::check_limit_switch(MotorID motor_id, bool direction) -> bool {
    return hw_read_limit_switch(motor_id, direction);
}