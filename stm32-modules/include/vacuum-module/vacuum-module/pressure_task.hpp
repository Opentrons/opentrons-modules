#pragma once
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
#include "pressure_ramp.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pressure_task {
using lps22df::LPS222DF;
using vacuum_pressure_sensor::MPRLL0025PA00001;

// The frequency the pressure control loop runs at.
static constexpr const uint32_t CONTROL_PERIOD_HZ = 100;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1 / CONTROL_PERIOD_HZ) * 1000;
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
    double target_pressure = 0.0F;  // Target Guage Pressure
    double current_pressure = 0.0F;
    double ramp_rate = 0;
    uint32_t duration_s = 0;
    bool vent_after = false;
    bool start_pump = false;

    PressureRamp rampgen;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Current PID loop
    uint32_t last_tick = 0;
};

const PressureControl pressure_control = {
    .target_pressure = 0.0f,
    .pid = PID{.kp = 1,
               .ki = 0.5,
               .kd = 0,
               .sampletime = CONTROL_PERIOD_MS * 1000,
               .windup_limit_high = 18000,
               .windup_limit_low = 0},
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
        : _message_queue(q), _task_registry(aggregator) {}
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

    template <PressureControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::PressureControlMessage& m,
                       Policy& policy) -> void {
        // Get delta time
        auto timestamp = policy.get_time_ms();
        auto delta_s =
            (timestamp - _pressure_control.last_tick) * MS_TO_SECONDS;
        _pressure_control.last_tick = timestamp;

        // TODO: add FIR filter for abs pressure.
        auto abs_a_pressure_mbar =
            std::get<MPRDriverType>(get_sensor(ABS_PRESSURE_A).driver)
                .read_pressure();
        // TODO: use difference in pressure between a and b to raise error if >
        // threhold. Dont do anything with it for now
        auto abs_b_pressure_mbar =
            std::get<MPRDriverType>(get_sensor(ABS_PRESSURE_B).driver)
                .read_pressure();
        static_cast<void>(abs_b_pressure_mbar);

        // TODO: figure out how often to read atm pressure
        auto atm_pressure_hpa =
            std::get<LPSDriverType>(get_sensor(ATM_PRESSURE).driver)
                .get_pressure();

        // Compute the new pwm with ramp rate
        double target_setpoint =
            _pressure_control.rampgen.update_setpoint(timestamp);
        auto guage_pressure = abs_a_pressure_mbar - atm_pressure_hpa;
        auto difference = target_setpoint - guage_pressure;
        auto rpm = _pressure_control.pid.compute(difference, delta_s);
        // TODO: clamp the rpm here to something sensible

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
        _pressure_control.start_pump = m.start_pump;

        // Update ramp rate generator
        // TODO: Do we need to stop pump when we update ramp rate?
        auto current_pressure = _pressure_control.current_pressure;
        auto timestamp = policy.get_time_ms();
        _pressure_control.rampgen.start_ramp(
            current_pressure, m.pressure_setpoint, m.ramp_rate, timestamp);
        // TODO: kick off pressure control here, or in sep gcode? maybe
        // StartPump? 0. set target pressure, ramp rate, etc
        // 1. start the pressure driving task (if not started)
        // 1. set the pwm
        // 2. start the pump
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::GetPressureStateMessage& m,
                       Policy& policy) -> void {
        auto msg = messages::GetPressureStateResponseMessage{
            .responding_to_id = m.id,
            .target_pressure = _pressure_control.target_pressure,
            .current_pressure = _pressure_control.current_pressure,
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

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized{false};

    PressureSensor _abs_pressure_a = abs_pressure_a;
    PressureSensor _abs_pressure_b = abs_pressure_b;
    PressureSensor _atm_pressure = atm_pressure;

    PressureControl _pressure_control = pressure_control;
};

}  // namespace pressure_task
