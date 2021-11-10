#pragma once
#include <memory>
#include <utility>

#include "thermocycler-refresh/host_comms_task.hpp"
#include "thermocycler-refresh/system_task.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "test/test_message_queue.hpp"
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
    auto get_tasks_aggregator() -> tasks::Tasks<TestMessageQueue>& {
        return task_aggregator;
    }
    auto get_system_policy() -> TestSystemPolicy& { return system_policy; }

    auto run_system_task() -> void { system_task.run_once(system_policy); }

  private:
    TaskBuilder();
    TestMessageQueue<host_comms_task::Message> host_comms_queue;
    host_comms_task::HostCommsTask<TestMessageQueue> host_comms_task;
    TestMessageQueue<system_task::Message> system_queue;
    system_task::SystemTask<TestMessageQueue> system_task;
    tasks::Tasks<TestMessageQueue> task_aggregator;
    TestSystemPolicy system_policy;
};
