#pragma once

class PID {
  public:
    PID() = delete;
    PID(double kp, double ki, double kd, double sampletime);
    PID(double kp, double ki, double kd, double sampletime,
        double windup_limit_high, double windup_limit_low);
    auto compute(double error) -> double;
    auto reset() -> void;
    [[nodiscard]] auto kp() const -> double;
    [[nodiscard]] auto ki() const -> double;
    [[nodiscard]] auto kd() const -> double;
    [[nodiscard]] auto sampletime() const -> double;
    [[nodiscard]] auto windup_limit_high() const -> double;
    [[nodiscard]] auto windup_limit_low() const -> double;
    [[nodiscard]] auto last_error() const -> double;
    [[nodiscard]] auto last_iterm() const -> double;
    auto arm_integrator_reset(double error) -> void;

  private:
    enum IntegratorResetTrigger { RISING, FALLING, NONE };
    double _kp;
    double _ki;
    double _kd;
    double _sampletime;
    double _windup_limit_high;
    double _windup_limit_low;
    double _last_error;
    double _last_iterm;
    IntegratorResetTrigger _reset_trigger = NONE;
};
