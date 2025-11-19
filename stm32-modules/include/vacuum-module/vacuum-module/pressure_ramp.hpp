#include <algorithm>
#include <cstdint>

static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double PRESSURE_RAMP_TOLERANCE = 0.001;

class PressureRamp {
  public:
    PressureRamp() = default;

    /**
     * Start a new pressure ramp profile.
     * @param start_pressure The current pressure reading when starting the
     * ramp.
     * @param target_pressure The final pressure the ramp should reach.
     * @param rate_per_second The desired ramp rate (e.g., 68 MBAR per second).
     * @param current_time_ms The current system time in milliseconds.
     */
    auto start_ramp(double start_pressure, double target_pressure,
                    double rate_per_second, uint32_t current_time_ms) -> void {
        _initial_pressure = start_pressure;
        _final_target_pressure = target_pressure;
        _ramp_rate_per_second = rate_per_second;
        _start_time_ms = current_time_ms;
        _is_ramping = true;
    }

    /**
     * Calculates the current target setpoint for the PID controller.
     * Must be called periodically in your control loop.
     * @param current_time_ms The current system time in milliseconds.
     * @return The smooth, calculated setpoint for the PID.
     */
    auto update_setpoint(uint32_t current_time_ms) -> double {
        if (!_is_ramping) {
            return _final_target_pressure;  // Already at target
        }

        // Calculate elapsed time in seconds (using doubles for precision)
        const double elapsed_seconds =
            static_cast<double>(current_time_ms - _start_time_ms) *
            MS_TO_SECONDS;

        // Calculate the magnitude of pressure change requested so far
        const double pressure_change_magnitude =
            _ramp_rate_per_second * elapsed_seconds;

        // Determine if we are ramping up or down
        double ramped_setpoint = 0;
        if (_final_target_pressure > _initial_pressure) {
            // Ramping Up: Clamp to the final target pressure
            ramped_setpoint = _initial_pressure + pressure_change_magnitude;
            ramped_setpoint = std::min(ramped_setpoint, _final_target_pressure);
        } else {
            // Ramping Down: Clamp to the final target pressure
            ramped_setpoint = _initial_pressure - pressure_change_magnitude;
            ramped_setpoint = std::max(ramped_setpoint, _final_target_pressure);
        }

        // If we've reached the final target, stop ramping
        if (std::abs(ramped_setpoint - _final_target_pressure) <
            PRESSURE_RAMP_TOLERANCE) {
            ramped_setpoint = _final_target_pressure;
            _is_ramping = false;  // Stop accumulating time/error
        }

        return ramped_setpoint;
    }

    [[nodiscard]] auto is_ramping() const -> bool { return _is_ramping; }

  private:
    double _initial_pressure = 0.0;
    double _final_target_pressure = 0.0;
    double _ramp_rate_per_second = 0.0;
    uint32_t _start_time_ms = 0;
    bool _is_ramping = false;
};
