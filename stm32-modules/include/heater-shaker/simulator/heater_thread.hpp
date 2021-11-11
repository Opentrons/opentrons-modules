#pragma once
#include <memory>
#include <thread>

#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace heater_thread {
using SimHeaterTask = heater_task::HeaterTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimHeaterTask>;
};  // namespace heater_thread
