#pragma once

class PID {
  public:
    PID() = delete;
    PID(float kp, float ki, float kd);
    PID(float kp, float ki, float kd, float windup_limit_high,
        float windup_limit_low);
    auto compute(float error) -> float;
    auto reset() -> void;
    [[nodiscard]] auto kp() const -> float;
    [[nodiscard]] auto ki() const -> float;
    [[nodiscard]] auto kd() const -> float;
    [[nodiscard]] auto windup_limit_high() const -> float;
    [[nodiscard]] auto windup_limit_low() const -> float;
    [[nodiscard]] auto integrator() const -> float;
    [[nodiscard]] auto last_error() const -> float;

  private:
    const float _kp;
    const float _ki;
    const float _kd;
    const float _windup_limit_high;
    const float _windup_limit_low;
    float _integrator;
    float _last_error;
};
