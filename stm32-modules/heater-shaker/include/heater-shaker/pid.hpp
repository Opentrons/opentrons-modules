#pragma once

class PID {
  public:
    PID() = delete;
    PID(double kp, double ki, double kd);
    PID(double kp, double ki, double kd, double windup_limit_high,
        double windup_limit_low);
    auto compute(double error) -> double;
    auto reset() -> void;
    [[nodiscard]] auto kp() const -> double;
    [[nodiscard]] auto ki() const -> double;
    [[nodiscard]] auto kd() const -> double;
    [[nodiscard]] auto windup_limit_high() const -> double;
    [[nodiscard]] auto windup_limit_low() const -> double;
    [[nodiscard]] auto integrator() const -> double;
    [[nodiscard]] auto last_error() const -> double;

  private:
    const double _kp;
    const double _ki;
    const double _kd;
    const double _windup_limit_high;
    const double _windup_limit_low;
    double _integrator;
    double _last_error;
};
