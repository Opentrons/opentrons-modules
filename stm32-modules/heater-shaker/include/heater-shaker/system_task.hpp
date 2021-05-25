/*
 * the primary interface to the host communications task
 */
#pragma once

#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace system_task {
constexpr size_t RESPONSE_LENGTH = 128;

using Message = messages::SystemMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as SystemTask<SomeQueueImpl> rather than
// SystemTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message> class SystemTask {
    using Queue = QueueImpl<Message>;

  public:
    explicit SystemTask(Queue& q) : message_queue(q), task_registry(nullptr) {}
    SystemTask(const SystemTask& other) = delete;
    auto operator=(const SystemTask& other) -> SystemTask& = delete;
    SystemTask(SystemTask&& other) noexcept = delete;
    auto operator=(SystemTask&& other) noexcept -> SystemTask& = delete;
    ~SystemTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace system_task
