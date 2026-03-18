#pragma once
#include <cmath>
#include <cstdint>

#include "systemwide.h"

namespace waste_detector {

enum class WasteFullError : uint8_t {
    NO_ERROR = 0,
    MAX_DELTA_TIC_ERROR = 1,
    OVER_TARGET_ERROR = 2,
    RISE_TOO_FAST_ERROR = 3,
    FAST_BASELINE_ERROR = 4,
    FIRST_RUN_SLOW_ERROR = 5,
    SUDDEN_BLOCKED_ERROR = 6,
    CUMMULATIVE_BLOCKED_ERROR = 7,
};

// Window thresholds: Measure from p1 depth to p2 depth
static constexpr const double WASTE_WINDOW_START_PCT = 0.10F;
static constexpr const double WASTE_WINDOW_END_PCT = 0.95F;
// Resets the baseline if the new target pressure is this percent difference
static constexpr const double BASELINE_DEPTH_RESET = 0.30F;
// Compare to the learned baseline. If it is N x faster than empty, it's full.
static constexpr const double BASELINE_FAST_FACTOR = 0.75F;
// Allowed upward drift per tick while in hold phase
static constexpr const double MAX_RISE_PER_TICK = 3.5;
// Total allowed rise before flag for slow build up case
static constexpr const double MAX_CUMULATIVE_RISE = 11.0;
// Pressure offset from target to be considered in "hold" state
static constexpr const double PRESSURE_TOLERANCE = 10.0F;
// Large negative = draining (air rush) - ignore
static constexpr const double MAX_DRAIN_RISE_PER_TICK = -150.0;
static constexpr const double MAX_DELTA_PER_TICK = 250.0;
// Additional EMI smoothing for waste detection
static constexpr const double SENSOR_ALPHA = 0.5F;
// The baseline the system starts with, this is measured by how fast the floater
// closes the pump hose hole when the waste is full.
static constexpr const uint32_t DEFAULT_BASELINE = 600;
// Hard Minimum: If it reaches p2 vacuum in <  this many ms, it's full.
static constexpr const uint32_t MIN_ALLOWABLE_WINDOW_TIME_MS = 700;
// If the ramp takes longer than this, we flag it as a stall or leak.
static constexpr const uint32_t MAX_ALLOWABLE_WINDOW_TIME_MS = 20000;
// Number of tics we need to be around the target pressure to enter hold phase
static constexpr const uint32_t NEAR_TARGET_TICS = 25;  // ~1s at 25Hz

class WasteDetector {
  public:
    WasteDetector() = default;

    auto check(uint32_t timestamp, double current_abs_mbar,
               double target_abs_mbar, double p_atm) -> WasteFullError {
        if (waste_full_) {
            return error;
        }

        error = WasteFullError::NO_ERROR;
        auto total_vacuum_range = p_atm - target_abs_mbar;
        auto last_p = smoothed_p_;
        // EMA smoothing
        if (smoothed_p_ == 0) {
            smoothed_p_ = current_abs_mbar;
        } else {
            smoothed_p_ = (SENSOR_ALPHA * current_abs_mbar) +
                          ((1.0F - SENSOR_ALPHA) * last_p);
        }
        auto current_p = smoothed_p_;

        // Continuous delta_p for full-run spike/stall (positive = drop)
        auto delta_p = last_p - current_p;
        if (delta_p > MAX_DELTA_PER_TICK) {
            waste_full_ = true;
            error = WasteFullError::MAX_DELTA_TIC_ERROR;
            return error;
        }

        // Phase detection: Switch to hold if near target for a bit
        if (std::abs(current_p - target_abs_mbar) < PRESSURE_TOLERANCE) {
            near_target_ticks_++;
            if (near_target_ticks_ > NEAR_TARGET_TICS) {  // ~1s at 25Hz
                in_ramp_phase_ = false;                   // Enter hold mode
            }
        } else {
            // Re-enter ramp if we drift too far from the target
            near_target_ticks_ = 0;
            in_ramp_phase_ = true;
        }

        // Ramp phase: Pressure Window + stall timeout
        if (in_ramp_phase_) {
            auto p_window_start =
                p_atm - (total_vacuum_range * WASTE_WINDOW_START_PCT);
            auto p_window_end =
                p_atm - (total_vacuum_range * WASTE_WINDOW_END_PCT);

            if (current_p < p_window_start && ramp_start_ms_ == 0) {
                if (current_p < p_window_end) {
                    waste_full_ = true;
                    error = WasteFullError::OVER_TARGET_ERROR;
                    return error;
                }
                ramp_start_ms_ = timestamp;
            }

            if (current_p <= p_window_end && ramp_start_ms_ != 0) {
                auto measured_time = timestamp - ramp_start_ms_;
                if (measured_time < MIN_ALLOWABLE_WINDOW_TIME_MS) {
                    waste_full_ = true;
                    error = WasteFullError::RISE_TOO_FAST_ERROR;
                } else if (baseline_captured_) {
                    if (measured_time <
                        (baseline_rise_time_ms_ * BASELINE_FAST_FACTOR)) {
                        waste_full_ = true;
                        error = WasteFullError::FAST_BASELINE_ERROR;
                        return error;
                    }
                    return WasteFullError::NO_ERROR;
                } else {
                    // Only learn if time is reasonable when empty
                    if (measured_time > MIN_ALLOWABLE_WINDOW_TIME_MS * 2 &&
                        measured_time < MAX_ALLOWABLE_WINDOW_TIME_MS / 2) {
                        baseline_rise_time_ms_ = measured_time;
                        baseline_captured_ = true;
                        return WasteFullError::NO_ERROR;
                    }
                    // First run was too slow, flag as full
                    waste_full_ = true;
                    error = WasteFullError::FIRST_RUN_SLOW_ERROR;
                }
                if (waste_full_) {
                    return error;
                };
                ramp_start_ms_ = 0;
            }
        } else {  // Hold phase: Detect waste full (blocked flow)

            // Ignore sudden inrush of air
            if (delta_p < MAX_DRAIN_RISE_PER_TICK) {
                cumulative_rise_ = 0.0;
                return WasteFullError::NO_ERROR;
            }

            // Smaller rise = potential full waste (blocked flow)
            if (delta_p < -MAX_RISE_PER_TICK) {
                waste_full_ = true;
                error = WasteFullError::SUDDEN_BLOCKED_ERROR;
                // Cumulative rise over time (slow blocked-flow back-pressure)
            } else if (delta_p < 0) {
                cumulative_rise_ -= delta_p;
                if (cumulative_rise_ > MAX_CUMULATIVE_RISE) {
                    waste_full_ = true;
                    error = WasteFullError::CUMMULATIVE_BLOCKED_ERROR;
                }
            } else {
                // Reset cumulative if drop due to normal fluctuations
                cumulative_rise_ = 0.0;
            }
        }
        return error;
    }

    auto reset() -> void {
        waste_full_ = false;
        ramp_start_ms_ = 0;
        near_target_ticks_ = 0;
        cumulative_rise_ = 0.0;
        in_ramp_phase_ = true;
        smoothed_p_ = 0.0;
        error = WasteFullError::NO_ERROR;
    }

    [[nodiscard]] auto get_error() -> WasteFullError { return error; }

    [[nodiscard]] auto baseline_captured() const -> bool {
        return baseline_captured_;
    }

    auto reset_baseline() -> void {
        baseline_captured_ = false;
        baseline_rise_time_ms_ = 0;
    }

  private:
    bool waste_full_ = false;
    uint32_t ramp_start_ms_ = 0;
    uint32_t baseline_rise_time_ms_ = DEFAULT_BASELINE;
    bool baseline_captured_ = false;
    bool in_ramp_phase_ = true;
    uint32_t near_target_ticks_ = 0;
    double cumulative_rise_ = 0.0;
    double smoothed_p_ = 0.0;
    WasteFullError error = WasteFullError::NO_ERROR;
};

}  // namespace waste_detector
