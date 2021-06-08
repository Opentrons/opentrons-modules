#pragma once
#include <cstdint>

#include "heater-shaker/errors.hpp"

class TestMotorPolicy {
  public:
    TestMotorPolicy();
    TestMotorPolicy(int16_t initial_rpm, int16_t initial_target_rpm,
                    int32_t initial_ramp_rate);
    ~TestMotorPolicy() = default;
    auto set_rpm(int16_t rpm) -> errors::ErrorCode;
    [[nodiscard]] auto get_current_rpm() const -> int16_t;
    [[nodiscard]] auto get_target_rpm() const -> int16_t;
    auto set_ramp_rate(int32_t new_ramp_rate) -> errors::ErrorCode;
    auto stop() -> void;

    auto homing_solenoid_disengage() -> void;
    auto homing_solenoid_engage(uint16_t current_ma) -> void;

    auto delay_ticks(uint16_t ticks) -> void;

    auto plate_lock_set_power(float power) -> void;
    auto plate_lock_disable() -> void;

    [[nodiscard]] auto test_solenoid_engaged() const -> bool;
    [[nodiscard]] auto test_solenoid_current() const -> uint16_t;

    auto set_pid_constants(double kp, double ki, double kd) -> void;

    auto test_set_current_rpm(int16_t current_rpm) -> void;
    [[nodiscard]] auto test_get_ramp_rate() -> int32_t;

    auto test_set_rpm_return_code(errors::ErrorCode code) -> void;
    auto test_set_ramp_rate_return_code(errors::ErrorCode code) -> void;
    [[nodiscard]] auto test_get_last_delay() const -> uint16_t;

    [[nodiscard]] auto test_plate_lock_get_power() const -> float;
    [[nodiscard]] auto test_plate_lock_enabled() const -> bool;

    [[nodiscard]] auto test_get_overridden_ki() const -> double;
    [[nodiscard]] auto test_get_overridden_kp() const -> double;
    [[nodiscard]] auto test_get_overridden_kd() const -> double;

  private:
    int16_t target_rpm;
    int16_t current_rpm;
    int32_t ramp_rate;
    errors::ErrorCode set_rpm_return = errors::ErrorCode::NO_ERROR;
    errors::ErrorCode set_ramp_rate_return = errors::ErrorCode::NO_ERROR;
    bool solenoid_engaged = false;
    uint16_t solenoid_current = 0;
    uint16_t last_delay = 0;
    float plate_lock_power = 0;
    bool plate_lock_enabled = false;
    double overridden_ki = 0.0;
    double overridden_kp = 0.0;
    double overridden_kd = 0.0;
};
