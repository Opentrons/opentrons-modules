#pragma once

#include "core/ack_cache.hpp"
#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "errors.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "systemwide.h"

namespace tof_sensor_task {
using Message = messages::TOFSensorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class TOFSensorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit TOFSensorTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q), _task_registry(aggregator), _initialized(false) {}
    TOFSensorTask(const TOFSensorTask& other) = delete;
    auto operator=(const TOFSensorTask& other) -> TOFSensorTask& = delete;
    TOFSensorTask(TOFSensorTask&& other) noexcept = delete;
    auto operator=(TOFSensorTask&& other) noexcept -> TOFSensorTask& = delete;
    ~TOFSensorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto run_once() -> void {
        if (!_task_registry) {
            return;
        }

        _initialized = true;
        auto message = Message(std::monostate());

        _message_queue.recv(&message);
        auto visit_helper = [this](auto& message) -> void {
            this->visit_message(message);
        };
        std::visit(visit_helper, message);
    }

  private:
    auto visit_message(const std::monostate& m) -> void {
        static_cast<void>(m);
    }

    auto visit_message(const messages::GetTOFSensorStatusMessage& m) -> void {
        static_cast<void>(m);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized;
};
};  // namespace tof_sensor_task
