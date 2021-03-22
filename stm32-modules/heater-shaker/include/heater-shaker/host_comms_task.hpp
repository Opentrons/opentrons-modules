/*
 * the primary interface to the host communications task
 */
#pragma once

#include <array>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace host_comms_task {

using Message = messages::HostCommsMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as HostCommsTask<SomeQueueImpl> rather than
// HeaterTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message> class HostCommsTask {
    using Queue = QueueImpl<Message>;

  public:
    explicit HostCommsTask(Queue& q)
        : message_queue(q), task_registry(nullptr) {}
    HostCommsTask(const HostCommsTask& other) = delete;
    auto operator=(const HostCommsTask& other) -> HostCommsTask& = delete;
    HostCommsTask(HostCommsTask&& other) noexcept = delete;
    auto operator=(HostCommsTask&& other) noexcept -> HostCommsTask& = delete;
    ~HostCommsTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace host_comms_task
