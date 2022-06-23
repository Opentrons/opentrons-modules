#pragma once
#include <memory>
#include <thread>

#include "simulator/periodic_data_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/tasks.hpp"
#include "thermocycler-gen2/thermal_plate_task.hpp"

namespace thermal_plate_thread {
using SimThermalPlateTask =
    thermal_plate_task::ThermalPlateTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build(
    std::shared_ptr<periodic_data_thread::PeriodicDataThread> periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimThermalPlateTask>;
};  // namespace thermal_plate_thread
