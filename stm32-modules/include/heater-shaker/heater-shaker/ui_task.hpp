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

namespace ui_task {
constexpr size_t RESPONSE_LENGTH = 128;

using Message = messages::UIMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as UITask<SomeQueueImpl> rather than
// HeaterTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class UITask {
  public:
    using Queue = QueueImpl<Message>;
    explicit UITask(Queue& q) : message_queue(q), task_registry(nullptr) {}
    UITask(const UITask& other) = delete;
    auto operator=(const UITask& other) -> UITask& = delete;
    UITask(UITask&& other) noexcept = delete;
    auto operator=(UITask&& other) noexcept -> UITask& = delete;
    ~UITask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace ui_task
