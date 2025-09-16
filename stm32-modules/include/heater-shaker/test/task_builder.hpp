#pragma once
#include <memory>
#include <utility>

#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/system_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "test/test_heater_policy.hpp"
#include "test/test_message_queue.hpp"
#include "test/test_motor_policy.hpp"
#include "test/test_system_policy.hpp"

struct TaskBuilder {
    using TestHostCommsQueue = TestMessageQueue<host_comms_task::Message>;
    using TestSystemQueue = TestMessageQueue<system_task::Message>;
    using TestMotorQueue = TestMessageQueue<motor_task::Message>;
    using TestHeaterQueue = TestMessageQueue<heater_task::Message>;
    using TestHeaterTask = heater_task::HeaterTask<TestMessageQueue>;
    using TestMotorTask = motor_task::MotorTask<TestMessageQueue>;
    using TestSystemTask = system_task::SystemTask<TestMessageQueue>;
    using TestHostCommsTask = host_comms_task::HostCommsTask<TestMessageQueue>;

    ~TaskBuilder() = default;
    static auto build() -> std::shared_ptr<TaskBuilder>;

    // Instances of this struct should only live in smart pointers and not
    // be passed around by-value
    TaskBuilder(const TaskBuilder&) = delete;
    auto operator=(const TaskBuilder&) -> TaskBuilder& = delete;
    TaskBuilder(TaskBuilder&&) noexcept = delete;
    auto operator=(TaskBuilder&&) noexcept -> TaskBuilder& = delete;

    template <typename Message,
              typename AckMessage = messages::AcknowledgePrevious>
    auto require_has_ack_for(
        Message message, errors::ErrorCode error = errors::ErrorCode::NO_ERROR)
        -> AckMessage {
        return require_has_ack_for_id<AckMessage>(message.id, error);
    }

    template <typename AckMessage = messages::AcknowledgePrevious>
    auto require_has_ack_for_id(
        uint32_t id, errors::ErrorCode error = errors::ErrorCode::NO_ERROR)
        -> AckMessage {
        auto ack = get_latest_host_comms_message<AckMessage>();
        REQUIRE(ack.responding_to_id == id);
        REQUIRE(ack.with_error == error);
        return ack;
    }

    template <typename Message>
    auto get_latest_host_comms_message() -> Message {
        CHECK_FALSE(host_comms_queue.backing_deque.empty());
        auto resp = host_comms_queue.backing_deque.front();
        CHECK(std::holds_alternative<Message>(resp));
        host_comms_queue.backing_deque.pop_front();
        return std::get<Message>(resp);
    }
    auto get_host_comms_queue() -> TestHostCommsQueue& {
        return host_comms_queue;
    }
    auto get_host_comms_task() -> TestHostCommsTask& { return host_comms_task; }
    auto get_system_queue() -> TestSystemQueue& { return system_queue; }
    auto get_system_task() -> TestSystemTask& { return system_task; }
    auto get_motor_queue() -> TestMotorQueue& { return motor_queue; }
    auto get_motor_task() -> TestMotorTask& { return motor_task; }
    auto get_heater_queue() -> TestHeaterQueue& { return heater_queue; }
    auto get_heater_task() -> TestHeaterTask& { return heater_task; }
    auto get_tasks_aggregator() -> tasks::Tasks<TestMessageQueue>& {
        return task_aggregator;
    }
    auto get_motor_policy() -> TestMotorPolicy& { return motor_policy; }

    auto get_heater_policy() -> TestHeaterPolicy& { return heater_policy; }

    auto run_heater_task() -> void { heater_task.run_once(heater_policy); }

    auto run_motor_task() -> void { motor_task.run_once(motor_policy); }

    auto get_system_policy() -> TestSystemPolicy& { return system_policy; }

    auto run_system_task() -> void { system_task.run_once(system_policy); }

    auto consume_heater_message(heater_task::Message message) -> void {
        heater_queue.backing_deque.push_back(message);
        run_heater_task();
        CHECK(heater_queue.backing_deque.empty());
    }
    auto consume_motor_message(motor_task::Message message) -> void {
        motor_queue.backing_deque.push_back(message);
        run_motor_task();
    }

  private:
    TaskBuilder();
    TestHostCommsQueue host_comms_queue;
    TestHostCommsTask host_comms_task;
    TestSystemQueue system_queue;
    TestSystemTask system_task;
    TestMotorQueue motor_queue;
    TestMotorTask motor_task;
    TestHeaterQueue heater_queue;
    TestHeaterTask heater_task;
    tasks::Tasks<TestMessageQueue> task_aggregator;
    TestMotorPolicy motor_policy;
    TestHeaterPolicy heater_policy;
    TestSystemPolicy system_policy;
};
