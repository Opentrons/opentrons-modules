#pragma once
#include <cstdint>

class TestMotorPolicy {
  public:
    TestMotorPolicy();
    TestMotorPolicy(int16_t initial_rpm, int16_t initial_target_rpm,
                    int32_t initial_ramp_rate);
    ~TestMotorPolicy() = default;
    auto set_rpm(int16_t rpm) -> void;
    [[nodiscard]] auto get_current_rpm() const -> int16_t;
    [[nodiscard]] auto get_target_rpm() const -> int16_t;
    auto set_ramp_rate(int32_t new_ramp_rate) -> void;
    auto stop() -> void;

    auto test_set_current_rpm(int16_t current_rpm) -> void;
    [[nodiscard]] auto test_get_ramp_rate() -> int32_t;

  private:
    int16_t target_rpm;
    int16_t current_rpm;
    int32_t ramp_rate;
};
