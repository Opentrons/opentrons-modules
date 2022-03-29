#pragma once
#include <boost/asio.hpp>

#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "thermocycler-gen2/host_comms_task.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace socket_sim_driver {

struct AddressInfo {
    std::string host;
    int port;
};

class SocketSimDriver : public sim_driver::SimDriver {
    static const std::string name;
    AddressInfo address_info;
    std::unique_ptr<boost::asio::ip::tcp::socket> s;

  public:
    SocketSimDriver(std::string);
    const std::string& get_host() const;
    int get_port() const;
    const std::string& get_name() const;

    void write(const std::string& message);
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace socket_sim_driver
