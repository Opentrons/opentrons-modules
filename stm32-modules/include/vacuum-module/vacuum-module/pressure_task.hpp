#pragma once
#include <algorithm>

#include "errors.hpp"
#include "pump_task.hpp"
#pragma GCC push_options
#pragma GCC optimize("O0")
#include <cmath>
#include <cstdint>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/pid.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pressure_policy.hpp"
#include "hal/message_queue.hpp"
#include "lps22df.hpp"
#include "messages.hpp"
#include "mprll0025pa00001a.hpp"
#include "systemwide.h"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pressure_task {
using lps22df::LPS222DF;
using vacuum_pressure_sensor::MPRLL0025PA00001;

constexpr uint8_t ABS_PRESSURE_A_ADDR = 0x18;
constexpr uint8_t ABS_PRESSURE_B_ADDR = 0x18;
constexpr uint8_t ATM_PRESSURE_ADDR = 0x5D;

// The frequency the pressure control loop runs at.
// static constexpr const uint32_t CONTROL_PERIOD_HZ = 100;
static constexpr const uint32_t CONTROL_PERIOD_HZ = 50;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1.0 / CONTROL_PERIOD_HZ) * 1000;
static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double RAMP_RATE_MBAR_S = 68.9476;

// Tuning Constants
const float ATM_PRESSURE_MBAR = 1013.0f;

// TUNING 1: Velocity Gain
// How much RPM to add for every 1 mbar/sec drop requested?
// Example: Dropping 500 mbar in 2 seconds (Rate = 250 mbar/s) needs 3000 RPM.
// K = 3000 / 250 = 12.0
const float K_VELOCITY = 40.0f;

// TUNING 2: Holding Gain
// Max RPM required to hold a deep vacuum against leaks.
const float K_HOLDING = 300.0f;

using MPRDriverType = MPRLL0025PA00001<i2c::hardware::I2C>;
using LPSDriverType = LPS222DF<i2c::hardware::I2C>;
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
    .driver = LPS222DF<i2c::hardware::I2C>(ATM_PRESSURE_ADDR),
};

struct PressureControl {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Pressure PID loop
    SlewRateLimiter slew;

    double target_pressure = 0;   // Target Guage Pressure
    double current_pressure = 0;  // Current Guage Pressure
    double prev_target_mbar = ATM_PRESSURE_MBAR;
    double ramp_rate = 0;
    double target_rpm = 0;
    uint32_t duration_s = 0;
    bool vent_after = false;

    double pressure_abs_a = 0;
    double pressure_abs_b = 0;
    double pressure_atm = 0;

    uint32_t last_tick = 0;
    bool enable_vacuum = false;
    bool vacuum_running = false;
    bool vent_opened = false;
};

const PressureControl pressure_control = {
    .pid = PID(0.5,                // kp
               0.39,                // ki
               0,                  // kd
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
            // Get vent state
            _pressure_control.vent_opened = policy.get_vent_state();
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

            // TODO: TEST THIS
            auto current_pressure = _pressure_control.pressure_abs_b;
            // _pressure_control.slew.configure(current_pressure, 50.0f); //
            // Slew rate is 25.0 mbar/sec
            _pressure_control.slew.configure(current_pressure, 40.0f);  // Slew rate is 25.0 mbar/sec

            // Close the vent
            policy.set_vent_state(true);

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

    auto send_debug_message(const char* message) -> void {
        if (_task_registry) {
            auto msg = messages::DebugMessage{.message = message};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

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

        // stop vacuum control
        if (!_pressure_control.enable_vacuum) {
            policy.start_pressure_control(false);

            // Stop pump control
            auto msg = messages::SetPumpStateMessage{.run_pump = false};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::PumpAddress));

            _pressure_control.pid.reset();
            _pressure_control.slew.reset();
            _pressure_control.target_rpm = 0;
            _pressure_control.last_tick = 0;
            return;
        }

        // update latest absolute pressure
        for (auto sensor_id : {ABS_PRESSURE_A, ABS_PRESSURE_B}) {
            // TODO: add FIR filter for abs pressure.
            auto ret = update_pressure(sensor_id);
            // Reset the sensor if there is some problem
            if (ret != NO_ERROR) {
                policy.sensor_reset(sensor_id);
                return;
            }
        }

        // Run Slew Limiter (Get smooth trajectory in mbar)
        // auto target_pressure = _pressure_control.target_pressure;
        // auto smooth_target = _pressure_control.slew.update(target_pressure, dt);
        auto smooth_target = _pressure_control.target_pressure;

        // Calculate Velocity Feed-Forward (Movement) Rate = Change in mbar /
        // Time (Previous - Current) because we want a positive RPM for a
        // pressure drop
        auto prev_target = _pressure_control.prev_target_mbar;
        auto rate_mbar_s = (prev_target - smooth_target) / dt;
        _pressure_control.prev_target_mbar = smooth_target;

        // Calculate RPM needed to achieve this flow rate
        auto ff_velocity = std::max(0.0, rate_mbar_s * K_VELOCITY);

        // 3. Calculate Holding Feed-Forward (Static Load)
        // Calculate how "deep" the vacuum is as a percentage (0.0 to 1.0)
        // 1013 mbar = 0% Vacuum, 0 mbar = 100% Vacuum
        auto ratio = (ATM_PRESSURE_MBAR - smooth_target) / ATM_PRESSURE_MBAR;
        ratio = std::clamp<double>(ratio, 0.0f, 1.0f);
        auto ff_holding = ratio * K_HOLDING;

        // If we are deeper than target, we don't need holding force, we need to
        // stop.
        auto current_abs = _pressure_control.pressure_abs_b;
        if (current_abs < smooth_target) {
            ff_holding = 0;
        }

        // 4. Total Feed Forward
        auto total_ff_rpm = ff_velocity + ff_holding;

        // Calculate target rpm
        _pressure_control.current_pressure = current_abs;
        auto error = current_abs - smooth_target;
        auto rpm = _pressure_control.pid.compute(error, dt);
        rpm = total_ff_rpm + rpm;

        // If we are significantly below target (deeper vacuum), cut the motor.
        // We add a small hysteresis (e.g. 5 mbar) to prevent jitter.
        if (current_abs < (smooth_target - 10.0f)) {
            rpm = 0;
        }

        // clamp rpm to max
        rpm = std::clamp<double>(rpm, MIN_RPM, MAX_RPM);
        _pressure_control.target_rpm = rpm;

        // Send new rpm to pump task
        auto msg = messages::SetPumpStateMessage{.rpm_setpoint = rpm,
                                                 .run_pump = true};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::PumpAddress));
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressureStateMessage& m,
                       Policy& policy) -> void {
        // TODO: Validate incoming values
        _pressure_control.target_pressure = m.pressure_setpoint;
        _pressure_control.ramp_rate = m.ramp_rate;
        _pressure_control.duration_s = m.duration_s;
        _pressure_control.vent_after = m.vent_after;

        // Start the pressure control loop
        if (!_pressure_control.enable_vacuum && m.start_pump) {
            update_pressure(ATM_PRESSURE);
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
            // NOTE: this updates the internal _pressure_control variables
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

        auto msg = messages::GetPressureStateResponseMessage{
            .responding_to_id = m.id,
            .target_pressure = _pressure_control.target_pressure,
            .current_pressure = _pressure_control.current_pressure,
            .pressure_abs_a = _pressure_control.pressure_abs_a,
            .pressure_abs_b = _pressure_control.pressure_abs_b,
            .pressure_atm = _pressure_control.pressure_atm,
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
        auto vent_state = !policy.get_vent_state();
        _pressure_control.vent_opened = vent_state;
        auto ret =
            vent_state == m.vent ? Error::NO_ERROR : Error::VENT_FAILED_ERROR;
        send_ack_message(m.id, ret);
    }

    auto update_pressure(PressureSensorID sensor_id) -> PressureSensorError {
        auto& sensor = get_sensor(sensor_id);
        if (!sensor.ok) {
            return DRIVER_INIT_ERROR;
        }

        // Request latest pressure readings
        // let this be optional
        auto pressure = std::visit(
            [&](auto&& driver) -> std::optional<double> {
                return driver.read_pressure().value();
            },
            sensor.driver);

        // Handle error
        if (!pressure.has_value()) {
            // TODO: Maybe return specific driver error
            return MATH_SATURATION_ERROR;
        }
        double pressure_val = pressure.value();
        // Update variables
        if (sensor_id == ABS_PRESSURE_A) {
            _pressure_control.pressure_abs_a = pressure_val;
        } else if (sensor_id == ABS_PRESSURE_B) {
            _pressure_control.pressure_abs_b = pressure_val;
        } else if (sensor_id == ATM_PRESSURE) {
            _pressure_control.pressure_atm = pressure_val;
        }

        return NO_ERROR;
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
#pragma GCC pop_options
