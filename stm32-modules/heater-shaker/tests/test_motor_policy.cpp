#include "test/test_motor_policy.hpp"

#include <cstdint>

TestMotorPolicy::TestMotorPolicy() : TestMotorPolicy(0, 0, 16) {}

TestMotorPolicy::TestMotorPolicy(int16_t initial_rpm,
                                 int16_t initial_target_rpm,
                                 int32_t initial_ramp_rate)
    : target_rpm(initial_target_rpm),
      current_rpm(initial_rpm),
      ramp_rate(initial_ramp_rate) {}

auto TestMotorPolicy::set_rpm(int16_t rpm) -> errors::ErrorCode {
    target_rpm = rpm;
    return set_rpm_return;
}

auto TestMotorPolicy::get_current_rpm() const -> int16_t { return current_rpm; }

auto TestMotorPolicy::get_target_rpm() const -> int16_t { return target_rpm; }

auto TestMotorPolicy::stop() -> void {
    current_rpm = 0;
    target_rpm = 0;
}

auto TestMotorPolicy::test_set_current_rpm(int16_t new_current_rpm) -> void {
    current_rpm = new_current_rpm;
}

auto TestMotorPolicy::set_ramp_rate(int32_t new_ramp_rate)
    -> errors::ErrorCode {
    ramp_rate = new_ramp_rate;
    return set_ramp_rate_return;
}

auto TestMotorPolicy::test_get_ramp_rate() -> int32_t { return ramp_rate; }

auto TestMotorPolicy::test_set_ramp_rate_return_code(errors::ErrorCode error)
    -> void {
    set_ramp_rate_return = error;
}

auto TestMotorPolicy::test_set_rpm_return_code(errors::ErrorCode error)
    -> void {
    set_rpm_return = error;
}


auto TestMotorPolicy::homing_solenoid_disengage() -> void {
    solenoid_engaged = false;
}

auto TestMotorPolicy::homing_solenoid_engage(uint16_t current_ma) -> void {
    solenoid_engaged = true;
    solenoid_current = current_ma;
}

auto TestMotorPolicy::test_solenoid_engaged() const -> bool {
    return solenoid_engaged;
}

auto TestMotorPolicy::test_solenoid_current() const -> uint16_t {
    return solenoid_current;
}
