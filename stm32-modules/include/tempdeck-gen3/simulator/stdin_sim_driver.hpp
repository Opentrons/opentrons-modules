#pragma once
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/simulator_tasks.hpp"
#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/messages.hpp"

namespace stdin_sim_driver {
class StdinSimDriver : public sim_driver::SimDriver {
    static const std::string name;

  public:
    StdinSimDriver();
    const std::string& get_name() const;
    void write(const std::string& message);
    void read(sim_driver::SendToCommsFunc&& send_to_comms);
};
}  // namespace stdin_sim_driver
