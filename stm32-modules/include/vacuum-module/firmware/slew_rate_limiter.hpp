#include <algorithm>
#include <cmath>

class SlewRateLimiter {
  public:
    SlewRateLimiter(double current = 0.0F, double rate_limit = 0.0F)
        : _current_output(current), _rate_limit(rate_limit) {}

    /**
     * @brief Configure the limiter for a new operation.
     * Use this when initializing the system or starting a new process.
     * * @param start_value The starting point (e.g. current actual pressure).
     * @param rate_limit_per_sec The max change allowed per second.
     */
    auto configure(double start_value, double rate_limit_per_sec) -> void {
        _current_output = start_value;
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Reset the internal integrator to a specific value.
     * Call this when the control loop resumes from Idle to prevent jumps.
     * Does not change the configured rate limit.
     */
    auto reset(double value = 0.0) -> void { _current_output = value; }

    /**
     * @brief Update the rate limit dynamically.
     * Useful if you have a "Fast Mode" and "Slow Mode".
     */
    auto set_rate_limit(double rate_limit_per_sec) -> void {
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Get the rate limit configured.
     */
    auto get_rate_limit() const -> double { return _rate_limit; }

    /**
     * @brief Calculates the next setpoint.
     * @param target The desired end goal.
     * @param dt_seconds Time elapsed since last call.
     * @return The smoothed output.
     */
    auto update(double target, double dt_seconds) -> double {
        auto const error = target - _current_output;
        auto const max_change = _rate_limit * dt_seconds;

        if (error > max_change) {
            _current_output += max_change;
        } else if (error < -max_change) {
            _current_output -= max_change;
        } else {
            _current_output = target;
        }

        return _current_output;
    }

    [[nodiscard]] auto get_current_setpoint() const -> double {
        return _current_output;
    }

  private:
    double _current_output = 0.0;
    double _rate_limit = 0.0;
};
