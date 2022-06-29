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
#include "core/ack_cache.hpp"

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
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    static constexpr size_t MY_ADDRESS = Queues::SystemAddress;\

    // Mark ID's for bootloader prep activities
    using BootloaderPrepCache = AckCache<4, int>;
  public:

    explicit SystemTask(Queue& q, Aggregator* aggregator = nullptr)
        : _message_queue(q), _task_registry(aggregator), _prep_cache() {}
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
    auto visit_message(const messages::SetSerialNumberMessage& message,
                       Policy& policy) {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        response.with_error = policy.set_serial_number(message.serial_number);
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostAddress));
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::EnterBootloaderMessage& message,
                       Policy& policy) {
        // Must disconnect USB before restarting
        auto id = _prep_cache.add(0);
        auto usb_msg = messages::ForceUSBDisconnect{.id = id, .return_address = MY_ADDRESS};
        if(!_task_registry->send_to_address(usb_msg, Queues::HostAddress)) {
            _prep_cache.remove_if_present(id);
        }

        if(_prep_cache.empty()) {
            // Couldn't send any messages? Enter bootloader anyways
            policy.enter_bootloader();
        }

        auto response = messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostAddress));
    }

    // Any Ack messages should be in response to bootloader prep messages
    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::AcknowledgePrevious& message,
                       Policy& policy) {
        auto res = _prep_cache.remove_if_present(message.responding_to_id);
        if(std::holds_alternative<std::monostate>(res)) {
            // We have no record of this id - ignore it
            return;
        }
        if(_prep_cache.empty()) {
            // All prep activities done, enter bootloader now
            policy.enter_bootloader();
        }
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    BootloaderPrepCache _prep_cache;
};

};  // namespace system_task
