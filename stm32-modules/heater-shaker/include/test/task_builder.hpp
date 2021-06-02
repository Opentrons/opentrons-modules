#pragma once
#include <memory>
#include <utility>

#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/system_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "test/test_heater_policy.hpp"
#include "test/test_message_queue.hpp"
#include "test/test_motor_policy.hpp"
#include "test/test_system_policy.hpp"

struct TaskBuilder {
    ~TaskBuilder() = default;

    static auto build() -> std::shared_ptr<TaskBuilder>;

    // Instances of this struct should only live in smart pointers and not
    // be passed around by-value
    TaskBuilder(const TaskBuilder&) = delete;
    auto operator=(const TaskBuilder&) -> TaskBuilder& = delete;
    TaskBuilder(TaskBuilder&&) noexcept = delete;
    auto operator=(TaskBuilder&&) noexcept -> TaskBuilder& = delete;

    auto get_host_comms_queue() -> TestMessageQueue<host_comms_task::Message>& {
        return host_comms_queue;
    }
    auto get_host_comms_task()
        -> host_comms_task::HostCommsTask<TestMessageQueue>& {
        return host_comms_task;
    }
    auto get_system_queue() -> TestMessageQueue<system_task::Message>& {
        return system_queue;
    }
    auto get_system_task() -> system_task::SystemTask<TestMessageQueue>& {
        return system_task;
    }
    auto get_motor_queue() -> TestMessageQueue<motor_task::Message>& {
        return motor_queue;
    }
    auto get_motor_task() -> motor_task::MotorTask<TestMessageQueue>& {
        return motor_task;
    }
    auto get_heater_queue() -> TestMessageQueue<heater_task::Message>& {
        return heater_queue;
    }
    auto get_heater_task() -> heater_task::HeaterTask<TestMessageQueue>& {
        return heater_task;
    }
    auto get_tasks_aggregator() -> tasks::Tasks<TestMessageQueue>& {
        return task_aggregator;
    }
    auto get_motor_policy() -> TestMotorPolicy& { return motor_policy; }

    auto get_heater_policy() -> TestHeaterPolicy& { return heater_policy; }

    auto run_heater_task() -> void { heater_task.run_once(heater_policy); }

    auto get_system_policy() -> TestSystemPolicy& { return system_policy; }

    auto run_system_task() -> void { system_task.run_once(system_policy); }

  private:
    TaskBuilder();
    TestMessageQueue<host_comms_task::Message> host_comms_queue;
    host_comms_task::HostCommsTask<TestMessageQueue> host_comms_task;
    TestMessageQueue<system_task::Message> system_queue;
    system_task::SystemTask<TestMessageQueue> system_task;
    TestMessageQueue<motor_task::Message> motor_queue;
    motor_task::MotorTask<TestMessageQueue> motor_task;
    TestMessageQueue<heater_task::Message> heater_queue;
    heater_task::HeaterTask<TestMessageQueue> heater_task;
    tasks::Tasks<TestMessageQueue> task_aggregator;
    TestMotorPolicy motor_policy;
    TestHeaterPolicy heater_policy;
    TestSystemPolicy system_policy;
};
