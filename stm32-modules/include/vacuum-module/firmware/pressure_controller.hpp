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
static constexpr const double SAMPLE_TIME_S = 0.04F;
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
static constexpr const double OVERSHOOT_DEPTH_PCT = 0.02F;
static constexpr const double MAX_OVERSHOOT_MBAR = 20.0F;
// Feed-forward tapers to zero within this band above the slewed trajectory.
static constexpr const double APPROACH_BAND_MBAR = 80.0F;
static constexpr const double ATM_PRESSURE_MBAR = 1013.25;

// Slew tunning
static constexpr const double MIN_RAMP_RATE = 0.20F;
static constexpr const double MAX_RAMP_RATE = 400.0F;
static constexpr const double DEFAULT_RAMP_RATE = 50;
// Slow the pressure slew within this fraction of total vacuum depth.
static constexpr const double ADAPTIVE_SLEW_END_FRACTION = 0.15F;

struct ConfigState {
    double ramp_rate = DEFAULT_RAMP_RATE;
    double k_velocity = K_VELOCITY;
    double k_holding = K_HOLDING;
    double overshoot = OVERSHOOT_ERROR;
    double approach_band = APPROACH_BAND_MBAR;
    double kp = KP;
    double ki = KI;
    double kd = KD;
};

class PressureController {
  public:
    PressureController(double sample_time_s = SAMPLE_TIME_S)
        : _pid(KP, KI, KD, sample_time_s, WINDUP_HIGH, WINDUP_LOW) {}

    auto configure_slew(double start_pressure, double ramp_rate) -> void {
        ramp_rate = ramp_rate > 0 ? ramp_rate : DEFAULT_RAMP_RATE;
        state.ramp_rate =
            std::clamp<double>(ramp_rate, MIN_RAMP_RATE, MAX_RAMP_RATE);
        _slew.configure(start_pressure, state.ramp_rate);
        prev_target_mbar_ = start_pressure;
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
        auto vacuum_depth = ATM_PRESSURE_MBAR - target_abs_mbar;
        if (vacuum_depth > 0.0) {
            auto remaining = std::max(0.0, current_abs_mbar - target_abs_mbar);
            auto depth_fraction = remaining / vacuum_depth;
            auto rate_scale = 1.0;
            if (depth_fraction < ADAPTIVE_SLEW_END_FRACTION) {
                rate_scale =
                    std::max(MIN_RAMP_RATE / state.ramp_rate,
                             depth_fraction / ADAPTIVE_SLEW_END_FRACTION);
            }
            _slew.set_rate_limit(state.ramp_rate * rate_scale);
        } else {
            _slew.set_rate_limit(state.ramp_rate);
        }

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
        auto effective_overshoot = compute_effective_overshoot(target_abs_mbar);
        auto is_overshot = (error < effective_overshoot);

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
            auto approach_scale =
                std::clamp(error / state.approach_band, 0.0, 1.0);
            total_ff_rpm = (ff_velocity + ff_holding) * approach_scale;
        }

        // Bleed integral windup as the trajectory is reached to limit
        // overshoot.
        if (error > 0.0) {
            _pid.arm_integrator_reset(error, std::abs(effective_overshoot));
        } else if (is_overshot) {
            // Do not let integral keep driving the pump past the trajectory.
            _pid.clear_integrator();
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

    [[nodiscard]] auto get_smooth_target() const -> double {
        return _slew.get_current_setpoint();
    }

    [[nodiscard]] auto compute_effective_overshoot(double target_abs_mbar) const
        -> double {
        auto vacuum_depth = ATM_PRESSURE_MBAR - target_abs_mbar;
        if (vacuum_depth <= 0.0) {
            return state.overshoot;
        }
        return -std::clamp(vacuum_depth * OVERSHOOT_DEPTH_PCT,
                           std::abs(state.overshoot), MAX_OVERSHOOT_MBAR);
    }

  private:
    PID _pid;
    SlewRateLimiter _slew;
    ConfigState state{};
    double prev_target_mbar_ = 0.0;
    double target_rpm = 0.0;
};

}  // namespace pressure_controller
