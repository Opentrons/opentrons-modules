#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace waste_detector {

// Waste-full is inferred, not measured. Sensors A and B sit in series
// inside the module, downstream of the waste-jug floater, so |A-B| is
// orifice flow after the jug.
//
// Full: tiny sealed volume. Hold is a deadhead (low conductance, no
// orifice flow). Empty leaky: higher RPM/depth in hold.

enum class WasteFullError : uint8_t {
    NO_ERROR = 0,
    FLOW_STABLE_FULL_ERROR = 1,
};

static constexpr const uint32_t CONTROL_PERIOD_HZ = 25;
static constexpr const uint32_t CONTROL_PERIOD_MS = 1000 / CONTROL_PERIOD_HZ;
static constexpr double MAX_DT_MS = static_cast<double>(CONTROL_PERIOD_MS * 4);
static constexpr const uint32_t NEAR_TARGET_MS = 2000;
static constexpr const double PRESSURE_TOLERANCE = 20.0F;
static constexpr const double SENSOR_ALPHA = 0.5F;

static constexpr double STABLE_HOLD_MS = 6000.0;
static constexpr double STABLE_HOLD_DEEP_MS = 10000.0;
static constexpr double DEEP_VACUUM_DEPTH_MBAR = 800.0;
static constexpr double FLOWING_DP_MBAR = 8.0;
static constexpr double MIN_WASTE_DEPTH_MBAR = 20.0;
// Commanded RPM per mbar of current vacuum.
static constexpr double G_SEALED_MAX = 0.50;

static constexpr const double ORIFICE_AREA = 0.00004536;  // m², 7.6 mm ID
static constexpr const double DISCHARGE_COEFF = 0.85;
static constexpr const double AIR_DENSITY = 1.2;  // kg/m³
static constexpr const double FLOW_RATE_ALPHA = 0.1F;
static constexpr const double FLOW_RATE_FACTOR = 1e6;
static constexpr const double MIN_DELTA_ALPHA = 0.1F;

struct WasteConfig {
    bool enable_waste_full = true;
    double p_filter_alpha = SENSOR_ALPHA;
    double g_sealed_max = G_SEALED_MAX;
    double flowing_dp_mbar = FLOWING_DP_MBAR;
    double stable_hold_ms = STABLE_HOLD_MS;
    double stable_hold_deep_ms = STABLE_HOLD_DEEP_MS;
    double min_waste_depth_mbar = MIN_WASTE_DEPTH_MBAR;
};

auto inline calculate_flow_per_second(double pressure_a, double pressure_b,
                                      double& smoothed_delta_p) -> double {
    auto delta_p_pa = std::abs(pressure_a - pressure_b) * 100.0;
    if (smoothed_delta_p == 0.0) {
        smoothed_delta_p = delta_p_pa;
    } else {
        smoothed_delta_p = (FLOW_RATE_ALPHA * delta_p_pa) +
                           ((1.0 - FLOW_RATE_ALPHA) * smoothed_delta_p);
    }

    if (smoothed_delta_p > MIN_DELTA_ALPHA) {
        const auto velocity = std::sqrt(2 * smoothed_delta_p / AIR_DENSITY);
        return DISCHARGE_COEFF * ORIFICE_AREA * velocity * FLOW_RATE_FACTOR;
    }
    return 0.0;
}

class WasteDetector {
  public:
    WasteDetector() = default;

    auto configure(WasteConfig c) -> void {
        config = c;
        config.p_filter_alpha = (c.p_filter_alpha <= 0.0)
                                    ? SENSOR_ALPHA
                                    : std::min(c.p_filter_alpha, 1.0);
    }

    auto check(uint32_t timestamp, double pressure_abs_a, double pressure_abs_b,
               double target_abs_mbar, double p_atm, double pump_rpm = 0.0)
        -> WasteFullError {
        if (!config.enable_waste_full) {
            return WasteFullError::NO_ERROR;
        }
        if (waste_full_) {
            return error;
        }
        if ((p_atm - target_abs_mbar) < config.min_waste_depth_mbar) {
            return WasteFullError::NO_ERROR;
        }

        flow_ml_per_s = calculate_flow_per_second(
            pressure_abs_a, pressure_abs_b, smoothed_delta_p);
        error = WasteFullError::NO_ERROR;

        if (smoothed_p_ == 0.0) {
            smoothed_p_ = pressure_abs_b;
        } else {
            smoothed_p_ = (config.p_filter_alpha * pressure_abs_b) +
                          ((1.0 - config.p_filter_alpha) * smoothed_p_);
        }
        const double current_p = smoothed_p_;
        const double vacuum_depth = p_atm - target_abs_mbar;
        const double current_vacuum = p_atm - current_p;
        const double orifice_dp = std::abs(pressure_abs_a - pressure_abs_b);
        const double g = conductance(pump_rpm, current_vacuum);

        const double dt_ms = sample_dt_ms(timestamp);

        const bool near_or_overshot =
            (std::abs(current_p - target_abs_mbar) < PRESSURE_TOLERANCE) ||
            (current_p < target_abs_mbar);
        if (!near_or_overshot) {
            near_target_ms_ = 0.0;
            sealed_hold_ms_ = 0.0;
            return WasteFullError::NO_ERROR;
        }

        near_target_ms_ += dt_ms;
        if (near_target_ms_ < NEAR_TARGET_MS) {
            return WasteFullError::NO_ERROR;
        }
        return check_hold(dt_ms, vacuum_depth, orifice_dp, g);
    }

    auto reset() -> void {
        waste_full_ = false;
        near_target_ms_ = 0.0;
        smoothed_p_ = 0.0;
        error = WasteFullError::NO_ERROR;
        sealed_hold_ms_ = 0.0;
        last_sample_ms_ = 0;
        have_sample_ = false;
        smoothed_delta_p = 0.0;
        flow_ml_per_s = 0.0;
    }

    auto reset_config() -> void {
        config.enable_waste_full = true;
        config.p_filter_alpha = SENSOR_ALPHA;
        config.g_sealed_max = G_SEALED_MAX;
        config.flowing_dp_mbar = FLOWING_DP_MBAR;
        config.stable_hold_ms = STABLE_HOLD_MS;
        config.stable_hold_deep_ms = STABLE_HOLD_DEEP_MS;
        config.min_waste_depth_mbar = MIN_WASTE_DEPTH_MBAR;
    }

    [[nodiscard]] auto get_error() const -> WasteFullError { return error; }
    [[nodiscard]] auto get_flow_rate() const -> double { return flow_ml_per_s; }
    [[nodiscard]] auto get_config() const -> WasteConfig { return config; }

  private:
    static auto conductance(double rpm, double vacuum_mbar) -> double {
        if (vacuum_mbar < 1.0) {
            return 0.0;
        }
        return rpm / vacuum_mbar;
    }

    auto sample_dt_ms(uint32_t timestamp) -> double {
        double dt_ms = 0.0;
        if (have_sample_) {
            dt_ms = std::min(static_cast<double>(timestamp - last_sample_ms_),
                             MAX_DT_MS);
        }
        have_sample_ = true;
        last_sample_ms_ = timestamp;
        return dt_ms;
    }

    auto trip(WasteFullError e) -> WasteFullError {
        waste_full_ = true;
        error = e;
        return error;
    }

    auto check_hold(double dt_ms, double vacuum_depth, double orifice_dp,
                    double g) -> WasteFullError {
        const bool sealed =
            (orifice_dp < config.flowing_dp_mbar) && (g < config.g_sealed_max);
        if (sealed) {
            sealed_hold_ms_ += dt_ms;
        } else if (sealed_hold_ms_ > dt_ms) {
            sealed_hold_ms_ -= dt_ms;
        } else {
            sealed_hold_ms_ = 0.0;
        }

        const double needed_ms = vacuum_depth >= DEEP_VACUUM_DEPTH_MBAR
                                     ? config.stable_hold_deep_ms
                                     : config.stable_hold_ms;
        if (sealed && sealed_hold_ms_ >= needed_ms) {
            return trip(WasteFullError::FLOW_STABLE_FULL_ERROR);
        }
        return WasteFullError::NO_ERROR;
    }

    bool waste_full_ = false;
    double smoothed_p_ = 0.0;
    uint32_t last_sample_ms_ = 0;
    double near_target_ms_ = 0.0;
    double sealed_hold_ms_ = 0.0;
    bool have_sample_ = false;
    double smoothed_delta_p = 0.0;
    double flow_ml_per_s = 0.0;
    WasteConfig config;
    WasteFullError error = WasteFullError::NO_ERROR;
};

}  // namespace waste_detector
