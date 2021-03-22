/*
 * the primary interface to the motor control task
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

namespace motor_task {
constexpr size_t RESPONSE_LENGTH = 128;
using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message> class MotorTask {
    using Queue = QueueImpl<Message>;

  public:
    explicit MotorTask(Queue& q) : message_queue(q), task_registry(nullptr) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace motor_task
