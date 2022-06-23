#pragma once
#include <memory>
#include <thread>

#include "simulator/periodic_data_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/lid_heater_task.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace lid_heater_thread {
using SimLidHeaterTask = lid_heater_task::LidHeaterTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build(
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimLidHeaterTask>;
};  // namespace lid_heater_thread
