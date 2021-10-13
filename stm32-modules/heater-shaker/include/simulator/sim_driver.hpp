#pragma once
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace sim_driver {

class SimDriver {
  public:
    virtual const std::string& get_name() const = 0;
    virtual void write(std::string message) = 0;
    virtual void read(tasks::Tasks<SimulatorMessageQueue>& tasks) = 0;
};
}  // namespace sim_driver