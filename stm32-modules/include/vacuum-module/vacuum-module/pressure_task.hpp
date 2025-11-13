#pragma once
#include <cmath>
#include <cstdint>
#include <variant>

#include "MPRLL0025PA00001A.hpp"
#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/atmosphere_pressure_sensor_policy.hpp"
#include "firmware/pressure_policy.hpp"
#include "firmware/vacuum_pressure_sensor_policy.hpp"
#include "hal/message_queue.hpp"
#include "lps22df.hpp"
#include "messages.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pressure_task {
using lps22df::LPS222DF;
using vacuum_pressure_sensor::MPRLL0025PA00001;

static constexpr const uint32_t CONTROL_PERIOD_MS = 3;

constexpr uint8_t ABS_PRESSURE_A_ADDR = 0x18;
constexpr uint8_t ABS_PRESSURE_B_ADDR = 0x18;
constexpr uint8_t ATM_PRESSURE_ADDR = 0x5C;

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

// const PressureSensor abs_pressure_b = {
//     .kind = ABS_PRESSURE_B,
//     .driver = MPRLL0025PA00001(ABS_PRESSURE_B_ADDR),
// };
//
// const PressureSensor atm_pressure = {
//     .kind = ATM_PRESSURE,
//     .driver = MPRLL0025PA00001(ATM_PRESSURE_ADDR),
// };

template <typename P>
concept PressureControlPolicy = requires(P p) {
    {p.sleep_ms(1)};
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
                        return driver.initialize(comms, sensor_id);
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
    auto visit_message(const messages::GetPressureMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PressureControlPolicy Policy>
    auto visit_message(const messages::SetTargetPressureMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    auto get_sensor(PressureSensorID sensor_id) -> PressureSensor& {
        switch (sensor_id) {
            case ABS_PRESSURE_A:
                return _abs_pressure_a;
            case ABS_PRESSURE_B:
                return _abs_pressure_a;
            //     return _abs_pressure_b;
            case ATM_PRESSURE:
                return _abs_pressure_a;
                // return _atm_pressure;
            default:
                return _abs_pressure_a;
        }
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized{false};

    PressureSensor _abs_pressure_a = abs_pressure_a;
    // PressureSensor _abs_pressure_b = abs_pressure_b;
    // PressureSensor _atm_pressure = atm_pressure;
};

}  // namespace pressure_task
