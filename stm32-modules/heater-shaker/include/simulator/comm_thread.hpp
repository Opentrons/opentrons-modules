#pragma once
#include <memory>
#include <thread>

#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/sim_driver.hpp"


namespace comm_thread {
using SimCommTask = host_comms_task::HostCommsTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimCommTask>;
void handle_input(sim_driver::SimDriver* driver, tasks::Tasks<SimulatorMessageQueue>& tasks);
};  // namespace comm_thread
