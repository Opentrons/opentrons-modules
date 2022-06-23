#pragma once
#include <memory>
#include <thread>

#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/system_task.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace system_thread {
using SimSystemTask = system_task::SystemTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimSystemTask>;
};  // namespace system_thread
