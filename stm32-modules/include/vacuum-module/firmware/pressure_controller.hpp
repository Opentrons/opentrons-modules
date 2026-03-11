#pragma GCC push_options
#pragma GCC optimize("O0")


#pragma once
#include <algorithm>
#include <cmath>

#include "core/pid.hpp"
#include "slew_rate_limiter.hpp"
#include "systemwide.h"

namespace pressure_controller {

static constexpr const double KP = 13.1;
static constexpr const double KI = 4.59;
static constexpr const double KD = 13.1;
static constexpr const double K_VELOCITY = 20.0;
static constexpr const double K_HOLDING = 43.0;
static constexpr const double OVERSHOOT_ERROR = -2.0;
static constexpr const double ATM_PRESSURE_MBAR = 1013.25;

struct ConfigState {
    double overshoot;
    double k_velocity;
    double k_holding;
};

class PressureController {
public:
    PressureController()
        : pid_(13.1, 4.59, 0.15, 40.0, MAX_RPM, 0) {}

    auto configure(double start_pressure_mbar, double ramp_rate_mbar_per_s) -> void {
        slew_.configure(start_pressure_mbar, ramp_rate_mbar_per_s);
    }

    auto update(double dt_seconds, double current_abs_mbar, double target_abs_mbar) -> double{
         // Run Slew Limiter to get smooth trajectory in mbar
        auto smooth_target = slew_.update(target_abs_mbar, dt_seconds);

        auto rate_mbar_s = (prev_target_mbar_ - smooth_target) / dt_seconds;
        auto error = current_abs_mbar - smooth_target;
        prev_target_mbar_ = smooth_target;

         // Apply Feed Forward if we are Pumping or Holding.
         // If we are Relaxing (Target is rising, rate is negative), we want 0
         // FF. If we are Overshot (Error is very negative), we want 0 FF.
        double total_ff_rpm = 0.0;
        bool is_relaxing = (rate_mbar_s < 0.0);
        bool is_overshot = (error < OVERSHOOT_ERROR);

        if (!is_relaxing && !is_overshot) {
            // Calculate RPM needed to achieve this flow rate (Pumping)
            auto ff_velocity = std::max(0.0, rate_mbar_s * K_VELOCITY);
            auto ratio = (ATM_PRESSURE_MBAR - smooth_target) / ATM_PRESSURE_MBAR;

             // Calculate Holding Feed-Forward (Static Load)
             // Calculate how "deep" the vacuum is as a percentage (0.0 to 1.0)
             // 1013 mbar = 0% Vacuum, 0 mbar = 100% Vacuum
            ratio = std::clamp(ratio, 0.0, 1.0);
            auto ff_holding = ratio * K_HOLDING;
            total_ff_rpm = ff_velocity + ff_holding;
        }

        // Calculate target rpm + safety clamp
        auto rpm = pid_.compute(error, dt_seconds);
        return std::clamp<double>(total_ff_rpm + rpm, MIN_RPM, MAX_RPM);
    }

    auto reset() -> void {
        pid_.reset();
        slew_.reset();
    }

    auto get_pid() -> PID {
        return pid_;
    }

    auto get_slew() -> SlewRateLimiter {
        return slew_;
    }

private:
    PID pid_;
    SlewRateLimiter slew_;
    ConfigState state;
    double target_pressure_ = 0.0;
    double prev_target_mbar_ = 0.0;
};

} // namespace pressure_controller
#pragma GCC pop_options
