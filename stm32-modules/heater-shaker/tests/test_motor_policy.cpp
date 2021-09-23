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

auto TestMotorPolicy::test_get_last_delay() const -> uint16_t {
    return last_delay;
}

auto TestMotorPolicy::delay_ticks(uint16_t ticks) -> void {
    last_delay = ticks;
}

auto TestMotorPolicy::plate_lock_set_power(float power) -> void {
    plate_lock_power = power;
    plate_lock_enabled = true;
}

auto TestMotorPolicy::plate_lock_disable() -> void {
    plate_lock_enabled = false;
}

auto TestMotorPolicy::test_plate_lock_get_power() const -> float {
    return plate_lock_power;
}

auto TestMotorPolicy::test_plate_lock_enabled() const -> bool {
    return plate_lock_enabled;
}

auto TestMotorPolicy::plate_lock_brake() -> void {
    plate_lock_braked = true;
}

auto TestMotorPolicy::test_plate_lock_braked() const -> bool {
    return plate_lock_braked;
}

auto TestMotorPolicy::plate_lock_open_sensor_read() const -> bool {
    if (!plate_lock_braked) {
        return false;
    } else {
        return plate_lock_power < 0.0 ? true : false;
    }
}

auto TestMotorPolicy::plate_lock_closed_sensor_read() const -> bool {
    if (!plate_lock_braked) {
        return false;
    } else {
        return plate_lock_power > 0.0 ? true : false;
    }
}

auto TestMotorPolicy::set_pid_constants(double kp, double ki, double kd)
    -> void {
    overridden_kp = kp;
    overridden_ki = ki;
    overridden_kd = kd;
}

auto TestMotorPolicy::test_get_overridden_kp() const -> double {
    return overridden_kp;
}

auto TestMotorPolicy::test_get_overridden_ki() const -> double {
    return overridden_ki;
}

auto TestMotorPolicy::test_get_overridden_kd() const -> double {
    return overridden_kd;
}
