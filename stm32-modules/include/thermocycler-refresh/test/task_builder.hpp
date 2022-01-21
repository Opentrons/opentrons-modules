#pragma once
#include <memory>
#include <utility>

#include "test/test_lid_heater_policy.hpp"
#include "test/test_message_queue.hpp"
#include "test/test_motor_policy.hpp"
#include "test/test_system_policy.hpp"
#include "test/test_thermal_plate_policy.hpp"
#include "thermocycler-refresh/host_comms_task.hpp"
#include "thermocycler-refresh/lid_heater_task.hpp"
#include "thermocycler-refresh/motor_task.hpp"
#include "thermocycler-refresh/system_task.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

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
    auto get_thermal_plate_queue()
        -> TestMessageQueue<thermal_plate_task::Message>& {
        return thermal_plate_queue;
    }
    auto get_thermal_plate_task()
        -> thermal_plate_task::ThermalPlateTask<TestMessageQueue>& {
        return thermal_plate_task;
    }
    auto get_lid_heater_queue() -> TestMessageQueue<lid_heater_task::Message>& {
        return lid_heater_queue;
    }
    auto get_lid_heater_task()
        -> lid_heater_task::LidHeaterTask<TestMessageQueue>& {
        return lid_heater_task;
    }

    auto get_motor_queue() -> TestMessageQueue<motor_task::Message>& {
        return motor_queue;
    }
    auto get_motor_task() -> motor_task::MotorTask<TestMessageQueue>& {
        return motor_task;
    }

    auto get_tasks_aggregator() -> tasks::Tasks<TestMessageQueue>& {
        return task_aggregator;
    }
    auto get_system_policy() -> TestSystemPolicy& { return system_policy; }
    auto get_thermal_plate_policy() -> TestThermalPlatePolicy& {
        return thermal_plate_policy;
    }
    auto get_lid_heater_policy() -> TestLidHeaterPolicy& {
        return lid_heater_policy;
    }

    auto get_motor_policy() -> TestMotorPolicy& { return motor_policy; }

    auto run_system_task() -> void { system_task.run_once(system_policy); }
    auto run_thermal_plate_task() -> void {
        thermal_plate_task.run_once(thermal_plate_policy);
    }
    auto run_lid_heater_task() -> void {
        lid_heater_task.run_once(lid_heater_policy);
    }

    auto run_motor_task() -> void { motor_task.run_once(motor_policy); }

  private:
    TaskBuilder();
    TestMessageQueue<host_comms_task::Message> host_comms_queue;
    host_comms_task::HostCommsTask<TestMessageQueue> host_comms_task;
    TestMessageQueue<system_task::Message> system_queue;
    system_task::SystemTask<TestMessageQueue> system_task;
    TestMessageQueue<thermal_plate_task::Message> thermal_plate_queue;
    thermal_plate_task::ThermalPlateTask<TestMessageQueue> thermal_plate_task;
    TestMessageQueue<lid_heater_task::Message> lid_heater_queue;
    lid_heater_task::LidHeaterTask<TestMessageQueue> lid_heater_task;
    TestMessageQueue<motor_task::Message> motor_queue;
    motor_task::MotorTask<TestMessageQueue> motor_task;
    tasks::Tasks<TestMessageQueue> task_aggregator;
    TestSystemPolicy system_policy;
    TestThermalPlatePolicy thermal_plate_policy;
    TestLidHeaterPolicy lid_heater_policy;
    TestMotorPolicy motor_policy;
};
