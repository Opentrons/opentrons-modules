#pragma once
#include <array>
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
    FLOW_STABLE_FULL_ERROR = 8,
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
static constexpr const double MAX_CUMULATIVE_RISE = 11.5;
// Pressure offset from target to be considered in "hold" state
static constexpr const double PRESSURE_TOLERANCE = 20.0F;
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
static constexpr const uint32_t NEAR_TARGET_TICS = 50;  // ~2s at 25Hz

// Standard Deviation
// ~800ms window for pressure std
static constexpr uint8_t PRESSURE_WINDOW_SIZE = 20;
// Oscillations above this means there is no air flow because the waste is full.
static constexpr double PRESSURE_OSCILLATION_MIN_STD = 7.5;

// Hose diam_m 0.0760m; ORIFICE_AREA = PI * (diam_m / 2.0) * (diam_m / 2.0)
static constexpr const double ORIFICE_AREA = 0.00004536;  // m²
static constexpr const double DISCHARGE_COEFF = 0.85;
static constexpr const double AIR_DENSITY = 1.2;  // kg/m³
// Flow rate EMI smoothing
static constexpr const double FLOW_RATE_ALPHA = 0.1F;
static constexpr const double FLOW_RATE_FACTOR = 1e6;
static constexpr const double MIN_DELTA_ALPHA = 0.1F;

struct WasteConfig {
    bool enable_waste_full = true;
    double p_window_start = WASTE_WINDOW_START_PCT;
    double p_window_end = WASTE_WINDOW_END_PCT;
    double baseline_fast_factor = BASELINE_FAST_FACTOR;
    double max_delta_per_tick = MAX_DELTA_PER_TICK;
    double max_rise_per_tick = MAX_RISE_PER_TICK;
    double max_cummulative_rise = MAX_CUMULATIVE_RISE;
    double p_filter_alpha = SENSOR_ALPHA;
    double min_window_time = MIN_ALLOWABLE_WINDOW_TIME_MS;
    double max_window_time = MAX_ALLOWABLE_WINDOW_TIME_MS;
};

auto inline calculate_flow_per_second(double pressure_a, double pressure_b,
                                      double smoothed_delta_p) -> double {
    auto delta_p_pa = std::abs(pressure_a - pressure_b) * 100.0;
    if (smoothed_delta_p == 0.0) {
        smoothed_delta_p = delta_p_pa;
    } else {
        smoothed_delta_p = (FLOW_RATE_ALPHA * delta_p_pa) +
                           ((1.0 - FLOW_RATE_ALPHA) * smoothed_delta_p);
    }

    // Prevent negative or tiny values
    if (smoothed_delta_p > MIN_DELTA_ALPHA) {
        // Bernoulli velocity
        const auto velocity = std::sqrt(2 * smoothed_delta_p / AIR_DENSITY);

        // Volumetric flow rate in ml/s
        return DISCHARGE_COEFF * ORIFICE_AREA * velocity * FLOW_RATE_FACTOR;
    }
    return 0.0;
}

// Helper: Calculate standard deviation of the fixed flow window
template <size_t N>
auto calculate_std_dev(const std::array<double, N>& buf, bool is_full,
                       uint8_t cur_idx) -> double {
    const uint8_t count = is_full ? N : cur_idx;
    if (count < 2) {
        return 0.0;
    };

    double mean = 0.0;
    for (uint8_t i = 0; i < count; ++i) {
        mean += buf.at(i);
    }
    mean /= count;

    double sum_sq_diff = 0.0;
    for (uint8_t i = 0; i < count; ++i) {
        const double diff = buf.at(i) - mean;
        sum_sq_diff += diff * diff;
    }
    return std::sqrt(sum_sq_diff / (count - 1));
}

class WasteDetector {
  public:
    WasteDetector() = default;

    auto configure(WasteConfig c) -> void {
        config.enable_waste_full = c.enable_waste_full;
        if (c.p_window_start > 0) {
            config.p_window_start = c.p_window_start;
        }
        if (c.p_window_end > 0) {
            config.p_window_end = c.p_window_end;
        }
        if (c.baseline_fast_factor > 0) {
            config.baseline_fast_factor = c.baseline_fast_factor;
        };
        if (c.max_delta_per_tick > 0) {
            config.max_delta_per_tick = c.max_delta_per_tick;
        }
        if (c.max_rise_per_tick > 0) {
            config.max_rise_per_tick = c.max_rise_per_tick;
        }
        if (c.max_cummulative_rise > 0) {
            config.max_cummulative_rise = c.max_cummulative_rise;
        }
        if (c.p_filter_alpha > 0) {
            config.p_filter_alpha = c.p_filter_alpha;
        }
        if (c.min_window_time > 0) {
            config.min_window_time = c.min_window_time;
        }
        if (c.max_window_time > 0) {
            config.max_window_time = c.max_window_time;
        }
    }

    auto check(uint32_t timestamp, double pressure_abs_a, double pressure_abs_b,
               double target_abs_mbar, double p_atm) -> WasteFullError {
        if (!config.enable_waste_full) {
            return WasteFullError::NO_ERROR;
        }
        if (waste_full_) {
            return error;
        }
        // Calculate flow rate
        flow_ml_per_s = calculate_flow_per_second(
            pressure_abs_a, pressure_abs_b, smoothed_delta_p);
        error = WasteFullError::NO_ERROR;
        auto total_vacuum_range = p_atm - target_abs_mbar;
        auto last_p = smoothed_p_;
        // EMA smoothing
        if (smoothed_p_ == 0) {
            smoothed_p_ = pressure_abs_b;
        } else {
            smoothed_p_ = (config.p_filter_alpha * pressure_abs_b) +
                          ((1.0F - config.p_filter_alpha) * smoothed_p_);
        }
        auto current_p = smoothed_p_;

        // Continuous delta_p for full-run spike/stall (positive = drop)
        auto delta_p = last_p - current_p;
        if (delta_p > config.max_delta_per_tick) {
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
                p_atm - (total_vacuum_range * config.p_window_start);
            auto p_window_end =
                p_atm - (total_vacuum_range * config.p_window_end);

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
                if (measured_time < config.min_window_time) {
                    waste_full_ = true;
                    error = WasteFullError::RISE_TOO_FAST_ERROR;
                } else if (baseline_captured_) {
                    if (measured_time < (baseline_rise_time_ms_ *
                                         config.baseline_fast_factor)) {
                        waste_full_ = true;
                        error = WasteFullError::FAST_BASELINE_ERROR;
                        return error;
                    }
                    return WasteFullError::NO_ERROR;
                } else {
                    // Only learn if time is reasonable when empty
                    if (measured_time > config.min_window_time * 2 &&
                        measured_time < config.max_window_time / 2) {
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

            // Add pressure to circular buffer
            pressure_window.at(pressure_window_index) = current_p;
            pressure_window_index =
                (pressure_window_index + 1) % PRESSURE_WINDOW_SIZE;
            if (pressure_window_index == 0) {
                pressure_window_full = true;
            }

            // Smaller rise = potential full waste (blocked flow)
            if (delta_p < -config.max_rise_per_tick) {
                waste_full_ = true;
                error = WasteFullError::SUDDEN_BLOCKED_ERROR;
                return error;
            }

            // Cumulative rise over time (slow blocked-flow back-pressure)
            if (delta_p < 0) {
                cumulative_rise_ -= delta_p;
                if (cumulative_rise_ > config.max_cummulative_rise) {
                    waste_full_ = true;
                    error = WasteFullError::CUMMULATIVE_BLOCKED_ERROR;
                }
            } else {
                // Reset cumulative if drop due to normal fluctuations
                cumulative_rise_ = 0.0;
            }

            // Check if we are osocilating more than we should, this
            // indicates no air-flow because the waste is full.
            if (pressure_window_full) {
                auto pressure_std =
                    calculate_std_dev(pressure_window, pressure_window_full,
                                      pressure_window_index);
                if (pressure_std > PRESSURE_OSCILLATION_MIN_STD) {
                    waste_full_ = true;
                    error = WasteFullError::FLOW_STABLE_FULL_ERROR;
                    return error;
                }
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
        pressure_window.fill(0);
        pressure_window_index = 0;
        pressure_window_full = false;
        smoothed_delta_p = 0.0;
        flow_ml_per_s = 0.0;
    }

    auto reset_baseline() -> void {
        baseline_captured_ = false;
        baseline_rise_time_ms_ = 0;
    }

    auto reset_config() -> void {
        config.enable_waste_full = true;
        config.p_window_start = WASTE_WINDOW_START_PCT;
        config.p_window_end = WASTE_WINDOW_END_PCT;
        config.baseline_fast_factor = BASELINE_FAST_FACTOR;
        config.max_delta_per_tick = MAX_DELTA_PER_TICK;
        config.max_rise_per_tick = MAX_RISE_PER_TICK;
        config.max_cummulative_rise = MAX_CUMULATIVE_RISE;
        config.p_filter_alpha = SENSOR_ALPHA;
        config.min_window_time = MIN_ALLOWABLE_WINDOW_TIME_MS;
        config.max_window_time = MAX_ALLOWABLE_WINDOW_TIME_MS;
    }

    [[nodiscard]] auto get_error() -> WasteFullError { return error; }
    [[nodiscard]] auto get_flow_rate() const -> double { return flow_ml_per_s; }
    [[nodiscard]] auto baseline_captured() const -> bool {
        return baseline_captured_;
    }

    auto get_config() -> WasteConfig { return config; }

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
    WasteConfig config;

    // standard deviation
    std::array<double, PRESSURE_WINDOW_SIZE> pressure_window = {0.0};
    uint8_t pressure_window_index = 0;
    bool pressure_window_full = false;
    double smoothed_delta_p = 0.0;
    double flow_ml_per_s = 0.0;
};

}  // namespace waste_detector
