#pragma once
#include <cstdint>

class TestMotorPolicy {
  public:
    TestMotorPolicy();
    TestMotorPolicy(int16_t initial_rpm, int16_t initial_target_rpm);
    ~TestMotorPolicy() = default;
    auto set_rpm(int16_t rpm) -> void;
    [[nodiscard]] auto get_current_rpm() const -> int16_t;
    [[nodiscard]] auto get_target_rpm() const -> int16_t;
    auto stop() -> void;

    auto test_set_current_rpm(int16_t current_rpm) -> void;

  private:
    int16_t target_rpm;
    int16_t current_rpm;
};
