#pragma once
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/host_comms_task.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace stdin_sim_driver {
class StdinSimDriver : public sim_driver::SimDriver {
    static const std::string name;

  public:
    StdinSimDriver();
    const std::string& get_name() const;
    void write(const std::string& message);
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace stdin_sim_driver
