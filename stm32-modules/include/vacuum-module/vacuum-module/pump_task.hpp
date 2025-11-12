#pragma once
#include <cmath>
#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pump_policy.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pump_task {
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
    auto visit_message(const messages::SetTargetRPMMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized{false};
};

}  // namespace pump_task
