#pragma GCC push_options
#pragma GCC optimize("O0")


#pragma once
#include <cmath>
#include <cstdint>

#include "systemwide.h"

namespace waste_detector {

static constexpr const double WASTE_WINDOW_START_PCT = 0.10;
static constexpr const double WASTE_WINDOW_END_PCT = 0.95;
static constexpr const double BASELINE_FAST_FACTOR = 0.75;
static constexpr const uint32_t MIN_ALLOWABLE_WINDOW_TIME_MS = 700;
static constexpr const uint32_t MAX_ALLOWABLE_WINDOW_TIME_MS = 20000;
static constexpr const double MAX_RISE_PER_TICK = 3.5;
static constexpr const double MAX_CUMULATIVE_RISE = 11.0;
static constexpr const double PRESSURE_TOLERANCE = 10.0;
static constexpr const double MAX_DRAIN_RISE_PER_TICK = -150.0;
static constexpr const double MAX_DELTA_PER_TICK = 250.0;
static constexpr const double SENSOR_ALPHA = 1.0F;

class WasteDetector {
public:
    WasteDetector() = default;

    auto reset() -> void {
        waste_full_ = false;
        ramp_start_ms_ = 0;
        near_target_ticks_ = 0;
        cumulative_rise_ = 0.0;
        in_ramp_phase_ = true;
        smoothed_p_ = 0.0;
    }

    auto check(uint32_t timestamp, double current_abs_mbar, double target_abs_mbar,
               double p_atm, double total_vacuum_range) -> int {
        if (waste_full_) {
            return 1;
            // return true;
        }

        // EMA smoothing
        // if (smoothed_p_ == 0) {
        //     smoothed_p_ = current_abs_mbar;
        // } else {
        //     smoothed_p_ = (SENSOR_ALPHA * current_abs_mbar) +
        //                             ((1.0F - SENSOR_ALPHA) * smoothed_p_);
        // }
        double current_p = current_abs_mbar;

        // Continuous delta_p for full-run spike/stall (positive = drop)
        static double last_p = 0.0;
        double delta_p = last_p - current_p;
        last_p = current_p;

        if (delta_p > MAX_DELTA_PER_TICK) {
            waste_full_ = true;
            return 2;
            // return true;
        }

        // Phase detection: Switch to hold if near target for a bit
        if (std::abs(current_p - target_abs_mbar) < PRESSURE_TOLERANCE) {
            near_target_ticks_++;
            if (near_target_ticks_ > 25) {  // ~1s at 25Hz
                in_ramp_phase_ = false;     // Enter hold mode
            }
        } else {
            // Re-enter ramp if we drift too far from the target
            near_target_ticks_ = 0;
            in_ramp_phase_ = true;
        }

        // Ramp phase: Pressure Window + stall timeout
        if (in_ramp_phase_) {
            double p_window_start = p_atm - (total_vacuum_range * WASTE_WINDOW_START_PCT);
            double p_window_end   = p_atm - (total_vacuum_range * WASTE_WINDOW_END_PCT);

            if (current_p <= p_window_start && ramp_start_ms_ == 0) {
                if (current_p <= p_window_end) {
                    waste_full_ = true;
                    return 3;
                    // return true;
                }
                ramp_start_ms_ = timestamp;
                return -1;
                // return false;
            }

            if (current_p <= p_window_end && ramp_start_ms_ != 0) {
                uint32_t measured_time = timestamp - ramp_start_ms_;
                if (measured_time < MIN_ALLOWABLE_WINDOW_TIME_MS) {
                    waste_full_ = true;
                    // return true;
                    return 4;
                } else if (baseline_captured_) {
                    if (measured_time < (baseline_rise_time_ms_ * BASELINE_FAST_FACTOR)) {
                        waste_full_ = true;
                        // return true;
                        return 5;
                    }
                } else {
                    // Only learn if time is reasonable when empty
                    if (measured_time > MIN_ALLOWABLE_WINDOW_TIME_MS * 1.5 &&
                        measured_time < MAX_ALLOWABLE_WINDOW_TIME_MS / 1.5) {
                        baseline_rise_time_ms_ = measured_time;
                        baseline_captured_ = true;
                        return -2;
                        // return false;
                    } else {
                        // First run was too slow, flag as full
                        waste_full_ = true;
                        // return true;
                        return 6;
                    }
                }
                ramp_start_ms_ = 0;
            }
        } else {  // Hold phase: Detect waste full (blocked flow)

            // Ignore sudden inrush of air
            if (delta_p < MAX_DRAIN_RISE_PER_TICK) {
                cumulative_rise_ = 0.0;
                // return false;
                return -3;
            }

            // Smaller rise = potential full waste (blocked flow)
            if (delta_p < -MAX_RISE_PER_TICK) {
                waste_full_ = true;
                // return true;
                return 7;
            }

            // Cumulative rise over time (slow blocked-flow back-pressure)
            if (delta_p < 0) {
                cumulative_rise_ -= delta_p;
                if (cumulative_rise_ > MAX_CUMULATIVE_RISE) {
                    waste_full_ = true;
                    // return true;
                    return 8;
                }
            } else {
                // Reset cumulative if drop due to normal fluctuations
                cumulative_rise_ = 0.0;
            }
        }
        // return false;
        return 0;
    }

private:
    bool waste_full_ = false;
    uint32_t ramp_start_ms_ = 0;
    uint32_t baseline_rise_time_ms_ = 600;
    bool baseline_captured_ = false;
    bool in_ramp_phase_ = true;
    uint32_t near_target_ticks_ = 0;
    double cumulative_rise_ = 0.0;
    double smoothed_p_ = 0.0;
};

} // namespace waste_detector
#pragma GCC pop_options
