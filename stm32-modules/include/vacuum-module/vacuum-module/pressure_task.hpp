/**
 * The Pressure Task is responsible for regulating pressure, it is the main
 * control loop which at a high-level, reads the pressure sensors, and sends
 * messages to the PumpTask to tell the motor what rpm is needed to maintain the
 * target pressure.
 *
 * 1. The entrypoint message is the `SetPressureStateMessage` which tells this
 * task the desired target pressure to maintain. In this message we start the
 * pressure control loop by calling the `policy.start_pressure_control` function
 * which starts a second FreeRTOS task that emits a `PressureControlMessage`
 * every `CONTROL_PERIOD_MS`.
 * 2. In the `PressureControlMessage`, we read the absolute pressure sensors
 *  from the motor (A) and manifold (B).
 * 3. We pass the abs readings through an EMA filter to smooth out the values
 * 4. The Slew Rate Limiter creates a smooth ramp to the target_pressure.
 * 5. We then calculate the error by subtracting the smooth target pressure
 *  from the current pressure which is fed to a PID that outputs base rpm.
 * 6. For the total rpm we use this base rpm and add on feed-forward velocity +
 * holding.
 * 7. A `SetPumpState` message is sent to the PumpTask with the new target RPM
 *  and the cycle is repeated on the next `PressureControlMessage` tick.
 *
 * Feed-Forward (FF) Math:
 * - Velocity FF: rpm = (mbar/sec_rate) * K_VELOCITY
 * Compensates for the dynamic load of changing pressure; provides the 'push'
 * to follow the ramp.
 * - Holding FF: rpm = [(P_atm - P_target) / P_atm] * K_HOLDING
 * Compensates for static atmospheric load. As vacuum deepens (ratio 0.0
 * -> 1.0), base RPM increases to counteract leaks and back-pressure. Safety: FF
 * is disabled during overshoot (error < -2.0mbar) or target relaxation to
 * prevent the pump from fighting the natural pressure rise.
 *
 * Stopping Pressure Control:
 * The pressure control is stopped when the `_pressure_control.enable_pump`
 * is false. We stop the `PressureControlMessage` emitter task, send a stop
 * `SetPumpMessage` message to the PumpTask, then reset all relevant internal
 *  _pressure_control variables.
 */

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/pid.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pressure_policy.hpp"
#include "hal/message_queue.hpp"
#include "lps22hb.hpp"
#include "messages.hpp"
#include "mprll0025pa00001a.hpp"
#include "slew_rate_limiter.hpp"
#include "systemwide.h"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pressure_task {
using lps22hb::LPS22HB;
using vacuum_pressure_sensor::MPRLL0025PA00001;

constexpr uint8_t ABS_PRESSURE_A_ADDR = 0x18;  // Closest to Manifold
constexpr uint8_t ABS_PRESSURE_B_ADDR = 0x18;  // Closest to Pump
constexpr uint8_t ATM_PRESSURE_ADDR = 0x5D;

// The frequency the pressure control loop runs at.
static constexpr const uint32_t CONTROL_PERIOD_HZ = 25;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1.0F / CONTROL_PERIOD_HZ) * 1000;
static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double ATM_PRESSURE_MBAR = 1013.25;
static constexpr const double MIN_RAMP_RATE = 0.20F;
static constexpr const double MAX_RAMP_RATE = 400.0F;
static constexpr const double DEFAULT_RAMP_RATE = 50;

// Velocity Gain
// How much RPM to add for every 1 mbar/sec drop requested.
static constexpr const double K_VELOCITY = 20.0F;
// Holding Gain:
// Max RPM required to hold a deep vacuum against leaks.
static constexpr const double K_HOLDING = 43.0F;
// Disables Velocity and Holding Gain if target is overshot
static constexpr const double OVERSHOOT_ERROR = -2.0F;

// -- Waste Detection Thresholds --
// Window thresholds: Measure from p1 depth to p2 depth
static constexpr const double WASTE_WINDOW_START_PCT = 0.10F;
static constexpr const double WASTE_WINDOW_END_PCT = 0.95F;
// Compare to the learned baseline. If it is N x faster than empty, it's full.
static constexpr const double BASELINE_FAST_FACTOR = 0.75F;
// Hard Minimum: If it reaches p2 vacuum in <  this many ms, it's full.
static constexpr const uint32_t MIN_ALLOWABLE_WINDOW_TIME_MS = 700;
// If the ramp takes longer than this, we flag it as a stall or leak.
static constexpr const uint32_t MAX_ALLOWABLE_WINDOW_TIME_MS = 20000;
// Allowed upward drift per tick while in hold phase
static constexpr const double MAX_RISE_PER_TICK = 3.5;
// Total allowed rise before flag for slow build up case
static constexpr const double MAX_CUMULATIVE_RISE = 11.0;
// Pressure offset from target to be considered in "hold" state
static constexpr const double PRESSURE_TOLERANCE = 10.0F;
// Large negative = draining (air rush) - ignore
static constexpr const double MAX_DRAIN_RISE_PER_TICK = -150.0;
// Debug buffer
std::array<char, 100> DEBUG_BUF;

using MPRDriverType = MPRLL0025PA00001<i2c::hardware::I2C>;
using LPSDriverType = LPS22HB<i2c::hardware::I2C>;
using Driver = std::variant<MPRDriverType, LPSDriverType>;

struct PressureSensor {
    PressureSensorID kind;
    Driver driver;
    PressureSensorState state = DISABLED;
    bool ok;
};

const PressureSensor abs_pressure_a = {
    .kind = ABS_PRESSURE_A,
    .driver = MPRLL0025PA00001<i2c::hardware::I2C>(ABS_PRESSURE_A_ADDR),
};

const PressureSensor abs_pressure_b = {
    .kind = ABS_PRESSURE_B,
    .driver = MPRLL0025PA00001<i2c::hardware::I2C>(ABS_PRESSURE_B_ADDR),
};

const PressureSensor atm_pressure = {
    .kind = ATM_PRESSURE,
    .driver = LPS22HB<i2c::hardware::I2C>(ATM_PRESSURE_ADDR),
};

struct PressureControl {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Pressure PID loop
    SlewRateLimiter slew;

    double target_pressure = 0;   // Target Guage Pressure
    double current_pressure = 0;  // Current Guage Pressure
    double prev_target_mbar = 0;
    double ramp_rate = 0;
    double target_rpm = 0;
    uint32_t duration_s = 0;
    float k_velocity = 0;
    float k_holding = 0;
    double overshoot_error = 0;
    bool vent_after = false;

    double pressure_abs_a = 0;
    double pressure_abs_b = 0;
    double pressure_atm = 0;

    uint32_t last_tick = 0;
    bool enable_vacuum = false;
    bool vent_opened = false;

    // --- Waste Detection Logic ---
    bool waste_full = false;
    bool slope_monitored_this_cycle = false;
    uint32_t ramp_start_ms = 0;
    double p_low_threshold = 0;
    double p_high_threshold = 0;
    double cumulative_rise = 0.0;

    // Baseline Rise Time to handle the "already full at startup"
    uint32_t baseline_rise_time_ms = 600;
    bool baseline_captured = false;
    bool in_ramp_phase = false;
    uint32_t near_target_ticks = 0;
};

const PressureControl pressure_control = {
    // Tuned for 25hz freq
    .pid = PID(13.1,               // kp
               4.59,               // ki
               0.15,               // kd
               CONTROL_PERIOD_MS,  // sampletime
               MAX_RPM,            // windup_limit_high
               0),                 // windup_limit_low
    .k_velocity = K_VELOCITY,
    .k_holding = K_HOLDING,
    .overshoot_error = OVERSHOOT_ERROR};

template <typename P>
concept PressureControlPolicy = requires(P p) {
    {p.sleep_ms(1)};
    { p.get_time_ms() } -> std::same_as<uint32_t>;
    {
        p.get_i2c_comms(PressureSensorID{})
        } -> std::same_as<i2c::hardware::I2C*>;
};

using PressurePolicy = pressure_policy::PressurePolicy;
using Message = messages::PressureMessage;
using Error = errors::ErrorCode;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class PressureTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit PressureTask(Queue& q, Aggregator* aggregator,
                          PressurePolicy* policy)
        : _message_queue(q), _task_registry(aggregator), _policy(policy) {}
    PressureTask(const PressureTask& other) = delete;
    auto operator=(const PressureTask& other) -> PressureTask& = delete;
    PressureTask(PressureTask&& other) noexcept = delete;
    auto operator=(PressureTask&& other) noexcept -> PressureTask& = delete;
    ~PressureTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <PressureControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            _policy = &policy;
            // Initialize pressure sensors
            for (auto sensor_id :
                 {ABS_PRESSURE_A, ABS_PRESSURE_B, ATM_PRESSURE}) {
                auto& sensor = get_sensor(sensor_id);
                sensor.state = INITIALIZING;
                auto comms = policy.get_i2c_comms(sensor_id);
                sensor.ok = std::visit(
                    [&](auto&& driver) -> bool {
                        auto ok = driver.initialize(comms, sensor_id);
                        if (ok) {
                            driver.read_pressure();
                        }
                        return ok;
                    },
                    sensor.driver);
                sensor.state = sensor.ok ? IDLE : SENSOR_ERROR;
            }

            // Slew rate is mbar/sec
            auto current_pressure =
                static_cast<float>(_pressure_control.pressure_abs_b);
            _pressure_control.slew.configure(current_pressure,
                                             DEFAULT_RAMP_RATE);

            // Close the vent
            set_vent_state(true);

            _message_queue.set_ready();
            _initialized = true;
        }

        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <PressureControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::PressureControlMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(m);
        // Get delta time
        auto timestamp = policy.get_time_ms();
        auto dt = (timestamp - _pressure_control.last_tick) * MS_TO_SECONDS;
        _pressure_control.last_tick = timestamp;

        // Stop vacuum control
        if (!_pressure_control.enable_vacuum) {
            policy.start_pressure_control(false);
            set_pump_state(false, 0);

            _pressure_control.pid.reset();
            _pressure_control.slew.reset();
            _pressure_control.current_pressure = 0;
            _pressure_control.target_rpm = 0;
            _pressure_control.last_tick = 0;
            return;
        }

        // Update latest absolute pressure
        for (auto sensor_id : {ABS_PRESSURE_B}) {
            auto ret = update_pressure(sensor_id);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                policy.sensor_reset(sensor_id);
                continue;
            }
        }

        auto current_pressure = _pressure_control.pressure_abs_b;
        _pressure_control.current_pressure = current_pressure;

        // Check and handle waste full error
        if (check_waste_state(timestamp)) {
            policy.start_pressure_control(false);
            set_vent_state(false);
            set_pump_state(false, 0);

            // TODO: Move this reset to a function
            _pressure_control.pid.reset();
            _pressure_control.slew.reset();
            _pressure_control.current_pressure = 0;
            _pressure_control.target_pressure = 0;
            _pressure_control.target_rpm = 0;
            _pressure_control.last_tick = 0;
            _pressure_control.ramp_start_ms = 0;
            _pressure_control.enable_vacuum = false;
            _pressure_control.slope_monitored_this_cycle = false;
            _pressure_control.in_ramp_phase = false;
            _pressure_control.cumulative_rise = 0.0;
            _pressure_control.near_target_ticks = 0.0;
            send_error_message(Error::WASTE_FULL_ERROR);
            return;
        }

        // Run Slew Limiter to get smooth trajectory in mbar
        auto target_pressure = _pressure_control.target_pressure;
        auto smooth_target = _pressure_control.slew.update(target_pressure, dt);

        auto prev_target = _pressure_control.prev_target_mbar;
        auto rate_mbar_s = (prev_target - smooth_target) / dt;
        auto error = current_pressure - smooth_target;
        _pressure_control.prev_target_mbar = smooth_target;

        // Apply Feed Forward if we are Pumping or Holding.
        // If we are Relaxing (Target is rising, rate is negative), we want 0
        // FF. If we are Overshot (Error is very negative), we want 0 FF.
        auto total_ff_rpm = 0.F;
        auto is_relaxing = (rate_mbar_s < 0.0F);
        auto is_overshot = (error < _pressure_control.overshoot_error);
        if (!is_relaxing && !is_overshot) {
            // Calculate RPM needed to achieve this flow rate (Pumping)
            auto ff_velocity = std::max<double>(
                0.0F, rate_mbar_s * _pressure_control.k_velocity);

            // Calculate Holding Feed-Forward (Static Load)
            // Calculate how "deep" the vacuum is as a percentage (0.0 to 1.0)
            // 1013 mbar = 0% Vacuum, 0 mbar = 100% Vacuum
            auto ratio =
                (ATM_PRESSURE_MBAR - smooth_target) / ATM_PRESSURE_MBAR;
            ratio = std::clamp<double>(ratio, 0.0F, 1.0F);
            auto ff_holding = ratio * _pressure_control.k_holding;
            total_ff_rpm = ff_velocity + ff_holding;
        }

        // Calculate target rpm
        auto rpm = _pressure_control.pid.compute(error, dt);
        rpm = total_ff_rpm + rpm;

        // Safety clamp
        rpm = std::clamp<double>(rpm, MIN_RPM, MAX_RPM);
        _pressure_control.target_rpm = rpm;
        set_pump_state(true, rpm);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressureStateMessage& m,
                       Policy& policy) -> void {
        // Convert target guage presure to abs pressure
        update_pressures(true);
        double p_atm = _pressure_control.pressure_atm;
        auto guage_pressure =
            std::clamp<double>(m.pressure_setpoint, ATM_PRESSURE_MBAR * -1, 0);
        auto target_pressure = guage_pressure + p_atm;

        // 3. Check for Significant Target Change (> 10% of the vacuum range)
        double current_target = _pressure_control.target_pressure;
        double vacuum_depth = std::abs(p_atm - target_pressure);
        double change_delta = std::abs(target_pressure - current_target);

        // If the target moves by more than 10% of the intended depth,
        // the previous baseline is no longer physically representative.
        if (_pressure_control.baseline_captured &&
            (change_delta > (vacuum_depth * 0.10))) {
            _pressure_control.baseline_captured = false;
            _pressure_control.baseline_rise_time_ms = 0;
        }

        auto ramp_rate = m.ramp_rate > 0 ? m.ramp_rate : DEFAULT_RAMP_RATE;
        ramp_rate = std::clamp<double>(ramp_rate, MIN_RAMP_RATE, MAX_RAMP_RATE);
        _pressure_control.target_pressure = target_pressure;
        _pressure_control.ramp_rate = ramp_rate;
        _pressure_control.duration_s = m.duration_s;
        _pressure_control.vent_after = m.vent_after;

        // Start the pressure control loop
        if (!_pressure_control.enable_vacuum && m.start_pump) {
            _pressure_control.ramp_start_ms = 0;
            _pressure_control.waste_full = false;
            _pressure_control.slope_monitored_this_cycle = false;
            _pressure_control.slew.configure(_pressure_control.pressure_abs_b,
                                             ramp_rate);
            policy.start_pressure_control(true);
        }
        _pressure_control.enable_vacuum = m.start_pump;

        send_ack_message(m.id);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetPressureStateMessage& m,
                       Policy& policy) -> void {
        // refresh if the pump is not running
        if (!_pressure_control.enable_vacuum) {
            for (auto sensor_id : {ABS_PRESSURE_B, ATM_PRESSURE}) {
                auto ret = update_pressure(sensor_id);
                // Reset the sensor if there is some problem
                if (ret != NO_ERROR) {
                    policy.sensor_reset(sensor_id);
                    continue;
                }
            }
        }

        // Convert to guage pressure
        auto target_pressure = 0.0F;
        auto current_pressure = 0.0F;
        if (_pressure_control.target_pressure > 0) {
            target_pressure = _pressure_control.target_pressure -
                              _pressure_control.pressure_atm;
            current_pressure = _pressure_control.current_pressure -
                               _pressure_control.pressure_atm;
        }
        auto msg = messages::GetPressureStateResponseMessage{
            .responding_to_id = m.id,
            .target_pressure = target_pressure,
            .current_pressure = current_pressure,
            .pressure_abs_a = _pressure_control.pressure_abs_a,
            .pressure_abs_b = _pressure_control.pressure_abs_b,
            .pressure_atm = _pressure_control.pressure_atm,
            .vacuum_enabled = _pressure_control.enable_vacuum,
            .vent_opened = _pressure_control.vent_opened,
        };
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetVentMessage& m, Policy& policy)
        -> void {
        // open/close the vent
        policy.set_vent_state(m.vent);
        auto vent_state = policy.get_vent_state();
        _pressure_control.vent_opened = vent_state;
        auto ret =
            vent_state == m.vent ? Error::NO_ERROR : Error::VENT_FAILED_ERROR;
        send_ack_message(m.id, ret);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressurePIDMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto& pc = _pressure_control;
        pc.overshoot_error = m.overshoot.value_or(pc.overshoot_error);
        pc.k_velocity = m.k_velocity.value_or(pc.k_velocity);
        pc.k_holding = m.k_holding.value_or(pc.k_holding);
        auto kp = m.kp.value_or(pc.pid.kp());
        auto ki = m.ki.value_or(pc.pid.ki());
        auto kd = m.kd.value_or(pc.pid.kd());
        pc.pid.set_tunings(kp, ki, kd, m.reset);
        send_ack_message(m.id);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetPressurePIDMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto msg = messages::GetPressurePIDResponseMessage{
            .responding_to_id = m.id,
            .kp = _pressure_control.pid.kp(),
            .ki = _pressure_control.pid.ki(),
            .kd = _pressure_control.pid.kd(),
            .overshoot = _pressure_control.overshoot_error,
            .k_velocity = _pressure_control.k_velocity,
            .k_holding = _pressure_control.k_holding,
        };
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    auto get_sensor(PressureSensorID sensor_id) -> PressureSensor& {
        switch (sensor_id) {
            case ABS_PRESSURE_A:
                return _abs_pressure_a;
            case ABS_PRESSURE_B:
                return _abs_pressure_b;
            case ATM_PRESSURE:
                return _atm_pressure;
            default:
                return _abs_pressure_a;
        }
    }

    auto update_pressures(bool reset_filter) -> void {
        for (auto sensor_id : {ABS_PRESSURE_B, ATM_PRESSURE}) {
            auto ret = update_pressure(sensor_id, reset_filter);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                _policy->sensor_reset(sensor_id);
                continue;
            }
        }
    }

    auto update_pressure(PressureSensorID sensor_id, bool reset_filter = false)
        -> PressureSensorError {
        auto& sensor = get_sensor(sensor_id);
        if (!sensor.ok) {
            return DRIVER_INIT_ERROR;
        }

        auto pressure = std::visit(
            [&](auto&& driver) -> double {
                return driver.read_pressure(reset_filter);
            },
            sensor.driver);

        // TODO: Handle error
        if (pressure < 0) {
            // TODO: Maybe return specific driver error
            return MATH_SATURATION_ERROR;
        }

        if (sensor_id == ABS_PRESSURE_A) {
            _pressure_control.pressure_abs_a = pressure;
        } else if (sensor_id == ABS_PRESSURE_B) {
            _pressure_control.pressure_abs_b = pressure;
        } else if (sensor_id == ATM_PRESSURE) {
            _pressure_control.pressure_atm = pressure;
        }

        return NO_ERROR;
    }

    auto set_vent_state(bool state) -> bool {
        _policy->set_vent_state(state);
        auto vent_state = _policy->get_vent_state();
        _pressure_control.vent_opened = vent_state;
        return vent_state == state;
    }

    auto set_pump_state(bool run_pump, double rpm = 0.0) -> void {
        auto msg = messages::SetPumpStateMessage{.rpm_setpoint = rpm,
                                                 .run_pump = run_pump};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::PumpAddress));
    }

    auto check_waste_state(uint32_t timestamp) -> bool {
        if (!_pressure_control.enable_vacuum || _pressure_control.waste_full) {
            return false;
        }

        double current_p = _pressure_control.pressure_abs_b;
        double p_atm = _pressure_control.pressure_atm;
        double target_abs = _pressure_control.target_pressure;
        double total_vacuum_range = p_atm - target_abs;
        if (total_vacuum_range < 100.0) {
            return false;
        }

        // Continuous delta_p for full-run spike/stall (positive = drop)
        static double last_p = 0.0;
        double delta_p = last_p - current_p;
        last_p = current_p;

        // Phase detection: Switch to hold if near target for a bit
        if (std::abs(current_p - target_abs) < PRESSURE_TOLERANCE) {
            _pressure_control.near_target_ticks++;
            if (_pressure_control.near_target_ticks > 25) {  // ~1s at 25Hz
                _pressure_control.in_ramp_phase = false;     // Enter hold mode
            }
        } else {
            // Re-enter ramp if we drift too far from the target
            _pressure_control.near_target_ticks = 0;
            _pressure_control.in_ramp_phase = true;
        }

        // Ramp phase: Pressure Window + stall timeout
        if (_pressure_control.in_ramp_phase) {
            double p_window_start =
                p_atm - (total_vacuum_range * WASTE_WINDOW_START_PCT);
            double p_window_end =
                p_atm - (total_vacuum_range * WASTE_WINDOW_END_PCT);

            if (current_p < p_window_start &&
                _pressure_control.ramp_start_ms == 0) {
                if (current_p < p_window_end) {
                    _pressure_control.waste_full = true;
                    return true;
                }
                _pressure_control.ramp_start_ms = timestamp;
            }

            if (current_p <= p_window_end &&
                _pressure_control.ramp_start_ms != 0) {
                uint32_t measured_time =
                    timestamp - _pressure_control.ramp_start_ms;
                if (measured_time < MIN_ALLOWABLE_WINDOW_TIME_MS) {
                    _pressure_control.waste_full = true;
                    return true;
                } else if (_pressure_control.baseline_captured) {
                    if (measured_time <
                        (_pressure_control.baseline_rise_time_ms *
                         BASELINE_FAST_FACTOR)) {
                        _pressure_control.waste_full = true;
                        return true;
                    }
                    return false;
                } else {
                    // Only learn if time is reasonable when empty
                    if (measured_time > MIN_ALLOWABLE_WINDOW_TIME_MS * 2 &&
                        measured_time < MAX_ALLOWABLE_WINDOW_TIME_MS / 2) {
                        _pressure_control.baseline_rise_time_ms = measured_time;
                        _pressure_control.baseline_captured = true;
                        return false;
                    } else {
                        // First run was too slow, flag as full
                        _pressure_control.waste_full = true;
                        return true;
                    }
                }
                _pressure_control.ramp_start_ms = 0;
                _pressure_control.slope_monitored_this_cycle = true;
                if (_pressure_control.waste_full) {
                    return true;
                }
            }
        } else {  // Hold phase: Detect waste full (blocked flow)

            // Ignore sudden inrush of air
            if (delta_p < MAX_DRAIN_RISE_PER_TICK) {
                _pressure_control.cumulative_rise = 0.0;
                return false;
            }

            // Smaller rise = potential full waste (blocked flow)
            if (delta_p < -MAX_RISE_PER_TICK) {
                _pressure_control.waste_full = true;
                return true;
            }
            // Cumulative rise over time (slow blocked-flow back-pressure)
            if (delta_p < 0) {
                _pressure_control.cumulative_rise -= delta_p;
                if (_pressure_control.cumulative_rise > MAX_CUMULATIVE_RISE) {
                    _pressure_control.waste_full = true;
                    return true;
                }
            } else {
                // Reset cumulative if drop due to normal fluctuations
                _pressure_control.cumulative_rise = 0.0;
            }
        }
        return false;
    }

    auto send_debug_message(const char* message) -> void {
        if (_task_registry) {
            std::strcpy(DEBUG_BUF.data(), message);
            auto msg = messages::DebugMessage{.message = DEBUG_BUF};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    auto send_error_message(Error error) -> void {
        if (_task_registry) {
            auto msg = messages::ErrorMessage{.code = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    auto send_ack_message(uint32_t response_id, Error error = Error::NO_ERROR)
        -> void {
        if (_task_registry) {
            auto msg = messages::AcknowledgePrevious{
                .responding_to_id = response_id, .with_error = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    PressurePolicy* _policy;
    bool _initialized{false};

    PressureSensor _abs_pressure_a = abs_pressure_a;
    PressureSensor _abs_pressure_b = abs_pressure_b;
    PressureSensor _atm_pressure = atm_pressure;

    PressureControl _pressure_control = pressure_control;
};

}  // namespace pressure_task
