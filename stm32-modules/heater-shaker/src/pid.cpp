#include "heater-shaker/pid.hpp"

#include <algorithm>
#include <limits>

PID::PID(float kp, float ki, float kd)
    : PID(kp, ki, kd, std::numeric_limits<float>::infinity(),
          -std::numeric_limits<float>::infinity()) {}

PID::PID(float kp, float ki, float kd, float windup_limit_high,
         float windup_limit_low)
    : _kp(kp),
      _ki(ki),
      _kd(kd),
      _windup_limit_high(windup_limit_high),
      _windup_limit_low(windup_limit_low),
      _integrator(0),
      _last_error(0) {}

auto PID::kp() const -> float { return _kp; }

auto PID::ki() const -> float { return _ki; }

auto PID::kd() const -> float { return _kd; }

auto PID::windup_limit_high() const -> float { return _windup_limit_high; }

auto PID::windup_limit_low() const -> float { return _windup_limit_low; }

auto PID::integrator() const -> float { return _integrator; }

auto PID::last_error() const -> float { return _last_error; }

auto PID::compute(float error) -> float {
    _integrator =
        std::clamp(error + _integrator, _windup_limit_low, _windup_limit_high);
    const float errdiff = error - _last_error;
    _last_error = error;
    return (_kp * error) + (_kd * errdiff) + (_ki * _integrator);
}

auto PID::reset() -> void {
    _integrator = 0;
    _last_error = 0;
}
