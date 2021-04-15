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

    static constexpr const uint32_t WAIT_TIME_TICKS = 100;

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

    /**
     * run_once() runs one spin of the task. This means it
     * - Waits for a message, or for a timeout so it can run its controller
     * - If there's a message, handles the message
     *   - which may include altering its controller state
     *   - which may include sending a response
     * - Runs its controller
     * */
    auto run_once() -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.try_recv(&message, WAIT_TIME_TICKS));
        std::visit(
            [this](const auto& msg) -> void { this->visit_message(msg); },
            message);
    }

  private:
    auto visit_message(const std::monostate& _ignore) -> void {
        static_cast<void>(_ignore);
    }

    auto visit_message(const messages::SetTemperatureMessage& msg) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    auto visit_message(const messages::GetTemperatureMessage& msg) -> void {
        auto response = messages::GetTemperatureResponse{
            .responding_to_id = msg.id,
            .current_temperature =
                99,  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
            .setpoint_temperature =
                48};  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace heater_task
