/*
 * the primary interface to the motor control task
 */
#pragma once

#include <concepts>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace motor_task {

/*
 * The MotorExecutionPolicy is how the portable task interacts
 * with the hardware. It is defined as a concept so it can be
 * passed as a reference paramter to run_once(), which means the
 * type of policy in actual use does not have to be part of the class's
 * type signature (which is used all over the place), just run_once's
 * type signature, which is used just by the rtos task and the test
 * harness.
 *
 * The policy exposes methods to get relevant data from the motor hardware
 * and methods to change the state of the motor controller.
 *
 * The policy is not the only way in which the hardware may interact
 * with the motor controller; it may also send messages. This should
 * be the way that the hardware sends information to the motor task
 * (as opposed to the motor task querying information from the hardware).
 * For instance, an asynchronous error mechanism should inform the motor
 * task of its event by sending a message.
 */
template <typename Policy>
concept MotorExecutionPolicy = requires(Policy& p, const Policy& cp) {
    {p.set_rpm(static_cast<int16_t>(16))};
    { cp.get_current_rpm() }
    ->std::same_as<int16_t>;
    { cp.get_target_rpm() }
    ->std::same_as<int16_t>;
    {p.stop()};
    {p.set_ramp_rate(static_cast<int32_t>(8))};
};

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

    template <typename Policy>
    requires MotorExecutionPolicy<Policy> auto run_once(Policy& policy)
        -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.try_recv(&message, WAIT_TIME_TICKS));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(_ignore);
        static_cast<void>(policy);
    }

    template <typename Policy>
    auto visit_message(const messages::SetRPMMessage& msg, Policy& policy)
        -> void {
        policy.set_rpm(msg.target_rpm);
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::SetAccelerationMessage& msg,
                       Policy& policy) -> void {
        policy.set_ramp_rate(msg.rpm_per_s);
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetRPMMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::GetRPMResponse{.responding_to_id = msg.id,
                                     .current_rpm = policy.get_current_rpm(),
                                     .setpoint_rpm = policy.get_target_rpm()};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::MotorSystemErrorMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        if (msg.errors == 0) {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(messages::ErrorMessage{
                        .code = errors::ErrorCode::MOTOR_SPURIOUS_ERROR})));
            return;
        }
        for (auto offset =
                 static_cast<uint8_t>(errors::MotorErrorOffset::FOC_DURATION);
             offset <= static_cast<uint8_t>(errors::MotorErrorOffset::SW_ERROR);
             offset++) {
            auto code = errors::from_motor_error(
                msg.errors, static_cast<errors::MotorErrorOffset>(offset));
            if (code != errors::ErrorCode::NO_ERROR) {
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        messages::HostCommsMessage(
                            messages::ErrorMessage{.code = code})));
            }
        }
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace motor_task
