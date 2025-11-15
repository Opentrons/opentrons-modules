#pragma once
#include <cmath>
#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/pid.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pump_policy.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pump_task {

// The frequency the pump control loop runs at.
static constexpr const uint32_t CONTROL_PERIOD_HZ = 100;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1 / CONTROL_PERIOD_HZ) * 1000;
static constexpr const double MS_TO_SECONDS = 0.001F;

struct PumpControl {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    double target_rpm = 0.0F;
    double current_rpm = 0.0F;

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Current PID loop
    uint32_t last_tick = 0;
    bool enable_pump = false;
    bool pump_running = false;
};

const PumpControl pump_control = {
    .target_rpm = 0.0f,
    .pid = PID{.kp = 1,
               .ki = 0.5,
               .kd = 0,
               .sampletime = CONTROL_PERIOD_MS * 1000,
               .windup_limit_high = 18000,
               .windup_limit_low = 0},
};

template <typename P>
concept PumpControlPolicy = requires(P p) {
    {p.sleep_ms(1)};
};

using PumpPolicy = pump_policy::PumpPolicy;
using Message = messages::PumpMessage;
using Error = errors::ErrorCode;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class PumpTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit PumpTask(Queue& q, Aggregator* aggregator, PumpPolicy* policy)
        : _message_queue(q), _task_registry(aggregator) {}
    PumpTask(const PumpTask& other) = delete;
    auto operator=(const PumpTask& other) -> PumpTask& = delete;
    PumpTask(PumpTask&& other) noexcept = delete;
    auto operator=(PumpTask&& other) noexcept -> PumpTask& = delete;
    ~PumpTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <PumpControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
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

    template <PumpControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::PumpControlMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);

        // Get delta time
        auto timestamp = policy.get_time_ms();
        auto delta_s = (timestamp - pump_control.last_tick) * MS_TO_SECONDS;
        _pump_control.last_tick = timestamp;

        // TODO: Do we want ramp gen here for smooth interpolation?
        // Compute the new duty cycle
        auto current_rpm = policy.get_pump_rpm();
        auto difference = _pump_control.target_rpm - current_rpm;
        auto duty = _pump_control.pid.compute(difference, delta_s);

        // set the motor duty cycle
        policy.set_pump_duty_cycle(duty);
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::SetPumpStateMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);

        // TODO: validate incoming values
        _pump_control.target_rpm = m.rpm_setpoint;
        _pump_control.enable_pump = m.run_pump;
        auto timestamp = policy.get_time_ms();
        (void)timestamp;

        if (!m.run_pump) {
            policy.enable_pump_control(false);
            policy.stop_pump_motor();
            // TODO: maybe check the rpm here and verify that the pump is off
            // Might want a way to ramp down when we turn off the pump.
            _pump_control.pump_running = false;
            return;
        }

        // start pump if not running
        if (m.run_pump && !_pump_control.pump_running) {
            policy.enable_pump_tach(true);
            policy.enable_pump_control(true);
            policy.start_pump_motor();
            _pump_control.pump_running = true;
        }
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::GetPumpStateMessage& m, Policy& policy)
        -> void {
        auto msg = messages::GetPumpStateResponseMessage{
            .responding_to_id = m.id,
            .target_rpm = _pump_control.target_rpm,
            .current_rpm = _pump_control.current_rpm,
            .pump_enabled = _pump_control.enable_pump,
        };
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized{false};

    PumpControl _pump_control = pump_control;
};

}  // namespace pump_task
