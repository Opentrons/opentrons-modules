#pragma once
#include <memory>
#include <thread>

#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace comm_thread {
using SimCommTask = host_comms_task::HostCommsTask<SimulatorMessageQueue>;
struct TaskControlBlock;
auto build() -> tasks::Task<std::unique_ptr<std::jthread>, SimCommTask>;
auto handle_input(tasks::Tasks<SimulatorMessageQueue>& tasks) -> void;
};  // namespace comm_thread
