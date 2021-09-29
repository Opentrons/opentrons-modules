#include <regex>
#include "simulator/sim_driver.hpp"

using namespace sim_driver;

sim_driver::SocketSimDriver::SocketSimDriver(std::string url) {
    std::regex url_regex (":\\/\\/([a-zA-Z0-9.]*):(\\d*)$");
    std::smatch url_match_result;

    if (std::regex_search(url, url_match_result, url_regex)) {
        host = url_match_result[1];
        port = std::stoi(url_match_result[2]);

    } else {
        std::cerr << "Malformed url." << std::endl;
        exit(1);
    }
}

std::string sim_driver::SocketSimDriver::get_host() { return this->host; }
int sim_driver::SocketSimDriver::get_port() { return this->port; }
std::string sim_driver::SocketSimDriver::get_name() { return this-> name; }
void sim_driver::SocketSimDriver::write() { }
std::string sim_driver::SocketSimDriver::read() { return "Hello World"; }

sim_driver::StdinSimDriver::StdinSimDriver() { }
std::string sim_driver::StdinSimDriver::get_name() { return this-> name; }
void sim_driver::StdinSimDriver::write() { }
std::string sim_driver::StdinSimDriver::read() { return "Hello World"; }
