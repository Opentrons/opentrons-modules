#include "heater-shaker/pid.hpp"

#include <algorithm>
#include <limits>

PID::PID(double kp, double ki, double kd)
    : PID(kp, ki, kd, std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()) {}

PID::PID(double kp, double ki, double kd, double windup_limit_high,
         double windup_limit_low)
    : _kp(kp),
      _ki(ki),
      _kd(kd),
      _windup_limit_high(windup_limit_high),
      _windup_limit_low(windup_limit_low),
      _integrator(0),
      _last_error(0) {}

auto PID::kp() const -> double { return _kp; }

auto PID::ki() const -> double { return _ki; }

auto PID::kd() const -> double { return _kd; }

auto PID::windup_limit_high() const -> double { return _windup_limit_high; }

auto PID::windup_limit_low() const -> double { return _windup_limit_low; }

auto PID::integrator() const -> double { return _integrator; }

auto PID::last_error() const -> double { return _last_error; }

auto PID::compute(double error) -> double {
    _integrator =
        std::clamp(error + _integrator, _windup_limit_low, _windup_limit_high);
    const double errdiff = error - _last_error;
    _last_error = error;
    return (_kp * error) + (_kd * errdiff) + (_ki * _integrator);
}

auto PID::reset() -> void {
    _integrator = 0;
    _last_error = 0;
}
