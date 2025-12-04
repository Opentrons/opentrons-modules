#pragma once
#include "errors.hpp"
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

// The frequency the pressure control loop runs at.
// static constexpr const uint32_t CONTROL_PERIOD_HZ = 100;
static constexpr const uint32_t CONTROL_PERIOD_HZ = 20;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1.0 / CONTROL_PERIOD_HZ) * 1000;
static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double RAMP_RATE_MBAR_S = 68.9476;

constexpr uint8_t ABS_PRESSURE_A_ADDR = 0x18;
constexpr uint8_t ABS_PRESSURE_B_ADDR = 0x18;
constexpr uint8_t ATM_PRESSURE_ADDR = 0x5D;

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

    double target_pressure = 0;   // Target Guage Pressure
    double current_pressure = 0;  // Current Guage Pressure
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
    .pid = PID(2.5,                // kp
               0.1,                // ki
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
        auto delta_s =
            (timestamp - _pressure_control.last_tick) * MS_TO_SECONDS;
        _pressure_control.last_tick = timestamp;

        // stop vacuum control
        if (!_pressure_control.enable_vacuum) {
            policy.start_pressure_control(false);

            // Stop pump control
            auto msg = messages::SetPumpStateMessage{.run_pump = false};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::PumpAddress));

            _pressure_control.pid.reset();
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

        // Calculate target rpm
        const double target_setpoint = _pressure_control.target_pressure;
        auto guage_pressure =
            _pressure_control.pressure_abs_b - _pressure_control.pressure_atm;
        _pressure_control.current_pressure = guage_pressure;
        auto difference = guage_pressure - target_setpoint;
        auto rpm = _pressure_control.pid.compute(difference, delta_s);

        // clamp rpm to max
        rpm = std::clamp<double>(rpm, MIN_RPM, MAX_RPM);
        _pressure_control.target_rpm = rpm;

        // Send new rpm to pump task
        auto msg = messages::SetPumpStateMessage{.rpm_setpoint = rpm,
                                                 .run_pump = true};
        // static_cast<void>(msg);
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::PumpAddress));

        // Send debug message
        send_debug_message("TEST");
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetPressureStateMessage& m,
                       Policy& policy) -> void {
        // TODO: Validate incoming values
        _pressure_control.target_pressure = m.pressure_setpoint;
        _pressure_control.ramp_rate = m.ramp_rate;
        _pressure_control.duration_s = m.duration_s;
        _pressure_control.vent_after = m.vent_after;

        // Update ramp rate generator
        // TODO: Do we need to stop pump when we update ramp rate?
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
        auto current_pressure = _pressure_control.current_pressure;
        auto pressure_abs_a = _pressure_control.pressure_abs_a;
        auto pressure_abs_b = _pressure_control.pressure_abs_b;
        auto pressure_atm = _pressure_control.pressure_atm;

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

            pressure_abs_a = _pressure_control.pressure_abs_a;
            pressure_abs_b = _pressure_control.pressure_abs_b;
            pressure_atm = _pressure_control.pressure_atm;
            current_pressure = pressure_abs_b - pressure_atm;

            // reset interals, TODO: deal with this
        }

        auto msg = messages::GetPressureStateResponseMessage{
            .responding_to_id = m.id,
            .target_pressure = _pressure_control.target_pressure,
            .current_pressure = current_pressure,
            .pressure_abs_a = pressure_abs_a,
            .pressure_abs_b = pressure_abs_b,
            .pressure_atm = pressure_atm,
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
        auto pressure = std::visit(
            [&](auto&& driver) -> double { return driver.read_pressure(); },
            sensor.driver);

        // Handle error
        if (pressure < 0) {
            // TODO: Maybe return specific driver error
            return MATH_SATURATION_ERROR;
        }

        // Update variables
        if (sensor_id == ABS_PRESSURE_A) {
            _pressure_control.pressure_abs_a = pressure;
        } else if (sensor_id == ABS_PRESSURE_B) {
            _pressure_control.pressure_abs_b = pressure;
        } else if (sensor_id == ATM_PRESSURE) {
            _pressure_control.pressure_atm = pressure;
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
