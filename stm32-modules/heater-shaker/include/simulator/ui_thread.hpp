#pragma once
#include <memory>
#include <thread>

#include "heater-shaker/tasks.hpp"
#include "heater-shaker/ui_task.hpp"
#include "simulator/simulator_queue.hpp"

namespace ui_thread {
using SimUITask = ui_task::UITask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimUITask>;
};  // namespace ui_thread
