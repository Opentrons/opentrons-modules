#pragma once
#include <boost/asio.hpp>

#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"

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
    std::string get_host();
    int get_port();
    const std::string& get_name() const;
    std::unique_ptr<boost::asio::ip::tcp::socket> get_socket();

    void write(std::string message);
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace socket_sim_driver