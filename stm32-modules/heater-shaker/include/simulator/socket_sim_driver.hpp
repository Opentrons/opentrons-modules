#pragma once
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"

namespace socket_sim_driver {

class SocketSimDriver : public sim_driver::SimDriver {
    static const std::string name;
    std::string host;
    int port;

  public:
    SocketSimDriver(std::string);
    std::string get_host();
    int get_port();
    const std::string& get_name() const;
    void write();
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace socket_sim_driver