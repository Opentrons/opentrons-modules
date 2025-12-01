#include <algorithm>
#include <cmath>

class SlewRateLimiter {
 public:
    // Default constructor: Initializes to safe zeros. 
    // You MUST call configure() before using update().
    SlewRateLimiter() : _current_output(0.0f), _rate_limit(0.0f) {}

    /**
     * @brief Configure the limiter for a new operation.
     * Use this when initializing the system or starting a new process.
     * * @param start_value The starting point (e.g. current actual pressure).
     * @param rate_limit_per_sec The max change allowed per second.
     */
    void configure(float start_value, float rate_limit_per_sec) {
        _current_output = start_value;
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Reset the internal integrator to a specific value.
     * Call this when the control loop resumes from Idle to prevent jumps.
     * Does not change the configured rate limit.
     */
    void reset(float value = 0.0) {
        _current_output = value;
    }

    /**
     * @brief Update the rate limit dynamically.
     * Useful if you have a "Fast Mode" and "Slow Mode".
     */
    void set_rate_limit(float rate_limit_per_sec) {
        _rate_limit = rate_limit_per_sec;
    }

    /**
     * @brief Calculates the next setpoint.
     * @param target The desired end goal.
     * @param dt_seconds Time elapsed since last call.
     * @return The smoothed output.
     */
    float update(float target, float dt_seconds) {
        float error = target - _current_output;
        float max_change = _rate_limit * dt_seconds;

        if (error > max_change) {
            _current_output += max_change;
        } else if (error < -max_change) {
            _current_output -= max_change;
        } else {
            _current_output = target;
        }

        return _current_output;
    }

    [[nodiscard]] float get_current_setpoint() const { return _current_output; }

 private:
    float _current_output;
    float _rate_limit;
};
