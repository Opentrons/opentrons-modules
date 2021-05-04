#include "test/test_motor_policy.hpp"

#include <cstdint>

TestMotorPolicy::TestMotorPolicy() : TestMotorPolicy(0, 0) {}

TestMotorPolicy::TestMotorPolicy(int16_t initial_rpm,
                                 int16_t initial_target_rpm)
    : target_rpm(initial_target_rpm), current_rpm(initial_rpm) {}

auto TestMotorPolicy::set_rpm(int16_t rpm) -> void { target_rpm = rpm; }

auto TestMotorPolicy::get_current_rpm() const -> int16_t { return current_rpm; }

auto TestMotorPolicy::get_target_rpm() const -> int16_t { return target_rpm; }

auto TestMotorPolicy::stop() -> void {
    current_rpm = 0;
    target_rpm = 0;
}

auto TestMotorPolicy::test_set_current_rpm(int16_t new_current_rpm) -> void {
    current_rpm = new_current_rpm;
}
