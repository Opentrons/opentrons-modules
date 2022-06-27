/**
 * @file system_task.hpp
 * @brief Primary interface for the system task
 *
 */
#pragma once

#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "hal/message_queue.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"

namespace system_task {

template <typename Policy>
concept SystemExecutionPolicy = requires(Policy& p) {
    {p.enter_bootloader()};
    {
        p.set_serial_number(std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
            "TESTSNXxxxxxxxxxxxxxxxx"})
        } -> std::same_as<errors::ErrorCode>;
    {
        p.get_serial_number()
        } -> std::same_as<std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>>;
};

using Message = messages::SystemMessage;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class SystemTask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;

    explicit SystemTask(Queue& q, Aggregator* aggregator = nullptr)
        : _message_queue(q), _task_registry(aggregator) {}
    SystemTask(const SystemTask& other) = delete;
    auto operator=(const SystemTask& other) -> SystemTask& = delete;
    SystemTask(SystemTask&& other) noexcept = delete;
    auto operator=(SystemTask&& other) noexcept -> SystemTask& = delete;
    ~SystemTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <SystemExecutionPolicy Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::GetSystemInfoMessage& message,
                       Policy& policy) {
        auto response = messages::GetSystemInfoResponse{
            .responding_to_id = message.id,
            .serial_number = policy.get_serial_number(),
            .fw_version = version::fw_version(),
            .hw_version = version::hw_version()};
        static_cast<void>(_task_registry->send(response));
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
};

};  // namespace system_task
