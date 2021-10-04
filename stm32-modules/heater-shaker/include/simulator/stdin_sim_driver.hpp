#pragma once
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"

namespace stdin_sim_driver {
class StdinSimDriver : public sim_driver::SimDriver {
    static const std::string name;

  public:
    StdinSimDriver();
    const std::string& get_name() const;
    void write();
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace stdin_sim_driver