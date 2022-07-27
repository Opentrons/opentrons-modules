#include "ot_utils/core/pid.hpp"

#include <algorithm>
#include <limits>

using namespace ot_utils::pid;

PID::PID(double kp, double ki, double kd, double sampletime)
    : PID(kp, ki, kd, sampletime, std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()) {}

PID::PID(double kp, double ki, double kd, double sampletime,
         double windup_limit_high, double windup_limit_low)
    : _kp(kp),
      _ki(ki),
      _kd(kd),
      _sampletime(sampletime),
      _windup_limit_high(windup_limit_high),
      _windup_limit_low(windup_limit_low),
      _last_error(0),
      _last_iterm(0) {}

auto PID::kp() const -> double { return _kp; }

auto PID::ki() const -> double { return _ki; }

auto PID::kd() const -> double { return _kd; }

auto PID::sampletime() const -> double { return _sampletime; }

auto PID::last_iterm() const -> double { return _last_iterm; }

auto PID::windup_limit_high() const -> double { return _windup_limit_high; }

auto PID::windup_limit_low() const -> double { return _windup_limit_low; }

auto PID::last_error() const -> double { return _last_error; }

auto PID::compute(double error) -> double {
    if (((_reset_trigger == FALLING) && (error <= 0)) ||
        ((_reset_trigger == RISING) && (error > 0))) {
        _last_iterm = 0;
        _reset_trigger = NONE;
    }
    const double unclamped_iterm = last_iterm() + sampletime() * ki() * error;
    const double iterm =
        std::clamp(unclamped_iterm, windup_limit_low(), windup_limit_high());
    _last_iterm = iterm;
    const double errdiff = error - last_error();
    _last_error = error;
    const double pterm = kp() * error;
    const double dterm = kd() * errdiff / sampletime();
    return pterm + iterm + dterm;
}

auto PID::compute(double error, double sampletime) -> double {
    _sampletime = sampletime;
    return compute(error);
}

auto PID::reset() -> void {
    _last_error = 0;
    _last_iterm = 0;
}

auto PID::arm_integrator_reset(double error) -> void {
    if (error <= 0) {
        _reset_trigger = RISING;
    } else {
        _reset_trigger = FALLING;
    }
}
