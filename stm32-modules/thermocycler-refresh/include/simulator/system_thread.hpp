#pragma once
#include <memory>
#include <thread>

#include "thermocycler-refresh/system_task.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace system_thread {
using SimSystemTask = system_task::SystemTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimSystemTask>;
};  // namespace system_thread
