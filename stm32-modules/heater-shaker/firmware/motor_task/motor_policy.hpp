#pragma once

#include <cstdint>

#include "mc_interface.h"

class MotorPolicy {
  public:
    static constexpr int32_t DEFAULT_RAMP_RATE_RPM_PER_S = 1000;
    MotorPolicy() = delete;
    explicit MotorPolicy(MCI_Handle_t *handle);
    auto set_rpm(int16_t rpm) -> void;
    [[nodiscard]] auto get_current_rpm() const -> int16_t;
    [[nodiscard]] auto get_target_rpm() const -> int16_t;
    auto stop() -> void;
    auto set_ramp_rate(int32_t rpm_per_s) -> void;

  private:
    int32_t ramp_rate = DEFAULT_RAMP_RATE_RPM_PER_S;
    MCI_Handle_t *motor_handle;
};
