#pragma once
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace sim_driver {

class SimDriver {
  public:
    virtual const std::string& get_name() const = 0;
    virtual void write() = 0;
    virtual void read(tasks::Tasks<SimulatorMessageQueue>& tasks) = 0;
};

class SocketSimDriver : public SimDriver {
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

class StdinSimDriver : public SimDriver {
    static const std::string name;

  public:
    StdinSimDriver();
    const std::string& get_name() const;
    void write();
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace sim_driver