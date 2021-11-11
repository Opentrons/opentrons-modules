#pragma once
#include <memory>
#include <thread>

#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace motor_thread {
using SimMotorTask = motor_task::MotorTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimMotorTask>;
};  // namespace motor_thread
