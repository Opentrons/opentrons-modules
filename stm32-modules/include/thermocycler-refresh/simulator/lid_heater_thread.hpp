#pragma once
#include <memory>
#include <thread>

#include "simulator/periodic_data_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-refresh/lid_heater_task.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace lid_heater_thread {
using SimLidHeaterTask = lid_heater_task::LidHeaterTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build(periodic_data_thread::PeriodicDataThread *periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimLidHeaterTask>;
};  // namespace lid_heater_thread
