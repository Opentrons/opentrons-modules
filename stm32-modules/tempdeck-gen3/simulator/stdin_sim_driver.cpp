#include "simulator/stdin_sim_driver.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <regex>

#include "simulator/simulator_queue.hpp"

using namespace stdin_sim_driver;

const std::string STDIN_DRIVER_NAME = "Stdin";

stdin_sim_driver::StdinSimDriver::StdinSimDriver() {}
const std::string stdin_sim_driver::StdinSimDriver::name = STDIN_DRIVER_NAME;
const std::string& stdin_sim_driver::StdinSimDriver::get_name() const {
    return this->name;
}
void stdin_sim_driver::StdinSimDriver::write(const std::string& message) {
    std::cout << message;
}
void stdin_sim_driver::StdinSimDriver::read(
    sim_driver::SendToCommsFunc&& send_to_comms) {
    auto linebuf = std::make_shared<std::string>(1024, 'c');
    while (true) {
        if (!std::cin.getline(linebuf->data(), linebuf->size() - 1, '\n')) {
            return;
        }
        auto wrote_to = std::cin.gcount();

        linebuf->at(wrote_to - 1) = '\n';
        auto message = messages::IncomingMessageFromHost(
            linebuf->data(), linebuf->data() + wrote_to);
        send_to_comms(message);
    }
}
