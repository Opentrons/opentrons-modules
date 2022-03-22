#pragma once
#include <memory>
#include <thread>

#include "simulator/periodic_data_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

namespace thermal_plate_thread {
using SimThermalPlateTask =
    thermal_plate_task::ThermalPlateTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build(periodic_data_thread::PeriodicDataThread *periodic_data)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimThermalPlateTask>;
};  // namespace thermal_plate_thread
