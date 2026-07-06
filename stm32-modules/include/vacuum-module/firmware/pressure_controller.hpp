#pragma once
#include <algorithm>
#include <cmath>

#include "core/pid.hpp"
#include "slew_rate_limiter.hpp"
#include "systemwide.h"

namespace pressure_controller {

// PID Tunnings
static constexpr const double KP = 13.1;
static constexpr const double KI = 4.59;
static constexpr const double KD = 0.15;
static constexpr const double SAMPLE_TIME = 40;
static constexpr const double WINDUP_HIGH = MAX_RPM;
static constexpr const double WINDUP_LOW = 0;
// Velocity Gain
// How much RPM to add for every 1 mbar/sec drop requested.
static constexpr const double K_VELOCITY = 20.0F;
// Holding Gain:
// Max RPM required to hold a deep vacuum against leaks.
static constexpr const double K_HOLDING = 43.0F;
// Disables Velocity and Holding Gain if target is overshot
static constexpr const double OVERSHOOT_ERROR = -2.0F;
static constexpr const double ATM_PRESSURE_MBAR = 1013.25;

// Slew tunning
static constexpr const double MIN_RAMP_RATE = 0.20F;
static constexpr const double MAX_RAMP_RATE = 400.0F;
static constexpr const double DEFAULT_RAMP_RATE = 50;

struct ConfigState {
    double ramp_rate = DEFAULT_RAMP_RATE;
    double k_velocity = K_VELOCITY;
    double k_holding = K_HOLDING;
    double overshoot = OVERSHOOT_ERROR;
    double kp = KP;
    double ki = KI;
    double kd = KD;
};

class PressureController {
  public:
    PressureController(double sample_time = SAMPLE_TIME)
        : _pid(KP, KI, KD, sample_time, WINDUP_HIGH, WINDUP_LOW) {}

    auto configure_slew(double start_pressure, double ramp_rate) -> void {
        ramp_rate = ramp_rate > 0 ? ramp_rate : DEFAULT_RAMP_RATE;
        state.ramp_rate =
            std::clamp<double>(ramp_rate, MIN_RAMP_RATE, MAX_RAMP_RATE);
        _slew.configure(start_pressure, state.ramp_rate);
    }

    auto configure_pid(double kp, double ki, double kd, double k_velocity,
                       double k_holding, double overshoot_error,
                       bool reset = false) -> void {
        state.k_velocity = k_velocity;
        state.k_holding = k_holding;
        state.overshoot = overshoot_error;
        state.kp = kp;
        state.ki = ki;
        state.kd = kd;
        _pid.set_tunings(kp, ki, kd, reset);
    }

    auto update(double dt_seconds, double current_abs_mbar,
                double target_abs_mbar) -> double {
        // Run Slew Limiter to get smooth trajectory in mbar
        auto smooth_target = _slew.update(target_abs_mbar, dt_seconds);

        auto rate_mbar_s = (prev_target_mbar_ - smooth_target) / dt_seconds;
        auto error = current_abs_mbar - smooth_target;
        prev_target_mbar_ = smooth_target;

        // Apply Feed Forward if we are Pumping or Holding.
        // If we are Relaxing (Target is rising, rate is negative), we want 0
        // FF. If we are Overshot (Error is very negative), we want 0 FF.
        auto total_ff_rpm = 0.0;
        auto is_relaxing = (rate_mbar_s < 0.0);
        auto is_overshot = (error < state.overshoot);

        if (!is_relaxing && !is_overshot) {
            // Calculate RPM needed to achieve this flow rate (Pumping)
            auto ff_velocity = std::max(0.0, rate_mbar_s * state.k_velocity);
            auto ratio =
                (ATM_PRESSURE_MBAR - smooth_target) / ATM_PRESSURE_MBAR;

            // Calculate Holding Feed-Forward (Static Load)
            // Calculate how "deep" the vacuum is as a percentage (0.0 to 1.0)
            // 1013 mbar = 0% Vacuum, 0 mbar = 100% Vacuum
            ratio = std::clamp(ratio, 0.0, 1.0);
            auto ff_holding = ratio * state.k_holding;
            total_ff_rpm = ff_velocity + ff_holding;
        }

        // Bleed integral windup as the trajectory is reached to limit overshoot.
        if (error > 0.0) {
            _pid.arm_integrator_reset(error, std::abs(state.overshoot));
        }

        // Calculate target rpm + safety clamp
        auto rpm = _pid.compute(error, dt_seconds);
        target_rpm = std::clamp<double>(total_ff_rpm + rpm, MIN_RPM, MAX_RPM);
        return target_rpm;
    }

    auto reset() -> void {
        _pid.reset();
        _slew.reset();
        prev_target_mbar_ = 0.0;
        target_rpm = 0.0;
    }

    auto get_state() -> ConfigState { return state; }

    [[nodiscard]] auto get_target_rpm() const -> double { return target_rpm; }

  private:
    PID _pid;
    SlewRateLimiter _slew;
    ConfigState state{};
    double prev_target_mbar_ = 0.0;
    double target_rpm = 0.0;
};

}  // namespace pressure_controller
