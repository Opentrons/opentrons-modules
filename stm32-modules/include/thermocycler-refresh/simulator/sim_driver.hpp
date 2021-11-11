#pragma once
#include "simulator/simulator_queue.hpp"
#include "thermocycler-refresh/host_comms_task.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace sim_driver {

class SimDriver {
  public:
    virtual const std::string& get_name() const = 0;
    virtual void write(const std::string& message) = 0;
    virtual void read(tasks::Tasks<SimulatorMessageQueue>& tasks) = 0;
};
}  // namespace sim_driver
