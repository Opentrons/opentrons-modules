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
    static constexpr const uint32_t WAIT_TIME_TICKS = 100;

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

    auto visit_message(const messages::SetRPMMessage& msg) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    auto visit_message(const messages::GetRPMMessage& msg) -> void {
        auto response = messages::GetRPMResponse{
            .responding_to_id = msg.id,
            .current_rpm =
                1050,  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
            .setpoint_rpm =
                3500};  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace motor_task
