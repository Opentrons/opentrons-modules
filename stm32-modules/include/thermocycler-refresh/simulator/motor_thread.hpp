#pragma once
#include <memory>
#include <thread>

#include "simulator/simulator_queue.hpp"
#include "thermocycler-refresh/motor_task.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace motor_thread {
using SimMotorTask = motor_task::MotorTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimMotorTask>;
};  // namespace motor_thread
