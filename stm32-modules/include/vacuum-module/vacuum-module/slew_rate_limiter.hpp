#include <algorithm>
#include <cmath>

class SlewRateLimiter {
  public:
    SlewRateLimiter(float current = 0.0F, float rate_limit = 0.0F)
        : _current_output(current), _rate_limit(rate_limit) {}

    /**
     * @brief Configure the limiter for a new operation.
     * Use this when initializing the system or starting a new process.
     * * @param start_value The starting point (e.g. current actual pressure).
     * @param rate_limit_per_sec The max change allowed per second.
     */
    auto configure(float start_value, float rate_limit_per_sec) -> void {
        _current_output = start_value;
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Reset the internal integrator to a specific value.
     * Call this when the control loop resumes from Idle to prevent jumps.
     * Does not change the configured rate limit.
     */
    auto reset(float value = 0.0) -> void { _current_output = value; }

    /**
     * @brief Update the rate limit dynamically.
     * Useful if you have a "Fast Mode" and "Slow Mode".
     */
    auto set_rate_limit(float rate_limit_per_sec) -> void {
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Calculates the next setpoint.
     * @param target The desired end goal.
     * @param dt_seconds Time elapsed since last call.
     * @return The smoothed output.
     */
    auto update(float target, float dt_seconds) -> float {
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

    [[nodiscard]] auto get_current_setpoint() const -> float {
        return _current_output;
    }

  private:
    float _current_output = 0.0;
    float _rate_limit = 0.0;
};
