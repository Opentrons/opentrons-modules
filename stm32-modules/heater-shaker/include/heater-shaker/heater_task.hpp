/*
 * the primary interface to the heater task
 */
#pragma once

#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace heater_task {
// By using a template template parameter here, we allow the code instantiating
// this template to do so as HeaterTask<SomeQueueImpl> rather than
// HeaterTask<SomeQueueImpl<Message>>
using Message = messages::HeaterMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message> class HeaterTask {
    using Queue = QueueImpl<Message>;

  public:
    explicit HeaterTask(Queue& q) : message_queue(q), task_registry(nullptr) {}
    HeaterTask(const HeaterTask& other) = delete;
    auto operator=(const HeaterTask& other) -> HeaterTask& = delete;
    HeaterTask(HeaterTask&& other) noexcept = delete;
    auto operator=(HeaterTask&& other) noexcept -> HeaterTask& = delete;
    ~HeaterTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }

    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace heater_task
