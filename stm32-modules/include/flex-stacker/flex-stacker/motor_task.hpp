/**
 * @file motor_task.hpp
 * @brief Primary interface for the motor task
 *
 */
#pragma once

#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
#include "hal/message_queue.hpp"

namespace motor_task {

template <typename P>
concept MotorControlPolicy = requires(P p, MotorID motor_id) {
    { p.enable_motor(motor_id) } -> std::same_as<bool>;
    { p.disable_motor(motor_id) } -> std::same_as<bool>;
};

using Message = messages::MotorMessage;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit MotorTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q), _task_registry(aggregator), _initialized(false) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <MotorControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        auto message = Message(std::monostate());

        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <MotorControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MotorEnableMessage& m, Policy& policy)
        -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        if (!(motor_enable(MotorID::MOTOR_X, m.x, policy) &&
              motor_enable(MotorID::MOTOR_Z, m.z, policy) &&
              motor_enable(MotorID::MOTOR_L, m.l, policy))) {
            response.with_error = m.x ? errors::ErrorCode::MOTOR_ENABLE_FAILED
                                      : errors::ErrorCode::MOTOR_DISABLE_FAILED;
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto motor_enable(MotorID id, std::optional<bool> engage, Policy& policy)
        -> bool {
        if (!engage.has_value()) {
            return true;
        }
        return engage ? policy.enable_motor(id) : policy.disable_motor(id);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveMotorAtFrequencyMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::StopMotorMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized;
};

};  // namespace motor_task
