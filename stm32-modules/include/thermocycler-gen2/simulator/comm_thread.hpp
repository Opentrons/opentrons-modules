#pragma once
#include <memory>
#include <thread>

#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/host_comms_task.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace comm_thread {
using SimCommTask = host_comms_task::HostCommsTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build(std::shared_ptr<sim_driver::SimDriver>&&)
    -> tasks::Task<std::unique_ptr<std::jthread>, SimCommTask>;
void handle_input(std::shared_ptr<sim_driver::SimDriver>&& driver,
                  tasks::Tasks<SimulatorMessageQueue>& tasks);
};  // namespace comm_thread
