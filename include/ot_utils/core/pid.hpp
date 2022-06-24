#pragma once

namespace ot_utils {
namespace pid {
/**
 * @brief Implements a starndard PID controller.
 */
class PID {
  public:
    PID() = delete;
    /**
     * @brief Create a PID controller without windup limits.
     *
     * @param[in] kp Proportional constant
     * @param[in] ki Integral constant
     * @param[in] kd Derivative constant
     * @param[in] sampletime The time between each sample, in seconds
     */
    PID(double kp, double ki, double kd, double sampletime);
    /**
     * @brief Create a PID controller without windup limits.
     *
     * @param[in] kp Proportional constant
     * @param[in] ki Integral constant
     * @param[in] kd Derivative constant
     * @param[in] sampletime The time between each sample, in seconds
     * @param[in] windup_limit_high High windup limit - the max positive
     * buildup of the integral term.
     * @param[in] windup_limit_low Low windup limit - the max negative
     * buildup of the integral term.
     */
    PID(double kp, double ki, double kd, double sampletime,
        double windup_limit_high, double windup_limit_low);
    /**
     * @brief Compute the output of the PID controller from a new
     * error value. Uses the last configured value of \ref sampletime
     *
     * @param[in] error The error in the input
     * @return double containing the output for the controller
     */
    auto compute(double error) -> double;
    /**
     * @brief Compute the output of the PID controller from a new
     * error value. The amount of time from the last error value
     * is used to scale the parameters
     *
     * @param[in] error The error in the input
     * @param[in] sampletime The time since the last input, in seconds
     * @return double containing the output for the controller
     */
    auto compute(double error, double sampletime) -> double;
    auto reset() -> void;
    [[nodiscard]] auto kp() const -> double;
    [[nodiscard]] auto ki() const -> double;
    [[nodiscard]] auto kd() const -> double;
    [[nodiscard]] auto sampletime() const -> double;
    [[nodiscard]] auto windup_limit_high() const -> double;
    [[nodiscard]] auto windup_limit_low() const -> double;
    [[nodiscard]] auto last_error() const -> double;
    [[nodiscard]] auto last_iterm() const -> double;
    auto arm_integrator_reset(double error) -> void;

  private:
    enum IntegratorResetTrigger { RISING, FALLING, NONE };
    double _kp;
    double _ki;
    double _kd;
    double _sampletime;
    double _windup_limit_high;
    double _windup_limit_low;
    double _last_error;
    double _last_iterm;
    IntegratorResetTrigger _reset_trigger = NONE;
};

}  // namespace pid
}  // namespace ot_utils
