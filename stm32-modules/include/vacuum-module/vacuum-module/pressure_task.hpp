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
static constexpr const double DEFAULT_RAMP_RATE = 400.0F;
// Velocity Gain:
// How much RPM to add for every 1 mbar/sec drop requested.
static constexpr const float K_VELOCITY = 20.0F;
// Holding Gain:
// Max RPM required to hold a deep vacuum against leaks.
static constexpr const float K_HOLDING = 43.0F;
// Disables Velocity and Holding Gain if target is overshot
static constexpr const double OVERSHOOT_ERROR = -2.0F;

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
    bool vent_after = false;

    double pressure_abs_a = 0;
    double pressure_abs_b = 0;
    double pressure_atm = 0;

    uint32_t last_tick = 0;
    bool enable_vacuum = false;
    bool vent_opened = false;
};

const PressureControl pressure_control = {
    // Tuned for 25hz freq
    .pid = PID(13.1,               // kp
               4.59,               // ki
               0.15,               // kd
               CONTROL_PERIOD_MS,  // sampletime
               MAX_RPM,            // windup_limit_high
               0),                 // windup_limit_low
};

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
            policy.set_vent_state(true);
            _pressure_control.vent_opened = policy.get_vent_state();

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
        for (auto sensor_id : {ABS_PRESSURE_A, ABS_PRESSURE_B}) {
            // TODO: add FIR filter for abs pressure.
            auto ret = update_pressure(sensor_id);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                policy.sensor_reset(sensor_id);
                continue;
            }
        }

        auto current_pressure = _pressure_control.pressure_abs_b;
        _pressure_control.current_pressure = current_pressure;

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
        auto is_overshot = (error < OVERSHOOT_ERROR);
        if (!is_relaxing && !is_overshot) {
            // Calculate RPM needed to achieve this flow rate (Pumping)
            auto ff_velocity = std::max<double>(0.0F, rate_mbar_s * K_VELOCITY);

            // Calculate Holding Feed-Forward (Static Load)
            // Calculate how "deep" the vacuum is as a percentage (0.0 to 1.0)
            // 1013 mbar = 0% Vacuum, 0 mbar = 100% Vacuum
            auto ratio =
                (ATM_PRESSURE_MBAR - smooth_target) / ATM_PRESSURE_MBAR;
            ratio = std::clamp<double>(ratio, 0.0F, 1.0F);
            auto ff_holding = ratio * K_HOLDING;
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
        update_pressure(ATM_PRESSURE);
        auto guage_pressure =
            std::clamp<double>(m.pressure_setpoint, ATM_PRESSURE_MBAR * -1, 0);
        auto target_pressure = guage_pressure + _pressure_control.pressure_atm;
        _pressure_control.target_pressure = target_pressure;
        _pressure_control.ramp_rate = m.ramp_rate;
        _pressure_control.duration_s = m.duration_s;
        _pressure_control.vent_after = m.vent_after;

        // Start the pressure control loop
        if (!_pressure_control.enable_vacuum && m.start_pump) {
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
            for (auto sensor_id :
                 {ABS_PRESSURE_A, ABS_PRESSURE_B, ATM_PRESSURE}) {
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

    auto update_pressure(PressureSensorID sensor_id) -> PressureSensorError {
        auto& sensor = get_sensor(sensor_id);
        if (!sensor.ok) {
            return DRIVER_INIT_ERROR;
        }

        auto pressure = std::visit(
            [&](auto&& driver) -> double { return driver.read_pressure(); },
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

    auto set_pump_state(bool run_pump, double rpm = 0.0) -> void {
        auto msg = messages::SetPumpStateMessage{.rpm_setpoint = rpm,
                                                 .run_pump = run_pump};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::PumpAddress));
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
