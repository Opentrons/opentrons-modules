#include <regex>
#include <boost/asio.hpp>
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"


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
void sim_driver::SocketSimDriver::read(tasks::Tasks<SimulatorMessageQueue>& tasks) {
    boost::asio::io_service io_context;

    boost::asio::ip::tcp::socket mysocket(io_context);
    boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string(this->host), this->port);
    boost::system::error_code ec;
    mysocket.connect(endpoint, ec);
    if (ec) {

    }

    char pBuff[30];
    boost::asio::mutable_buffer buff(pBuff, sizeof(pBuff));
    std::string tot;

    std::size_t l = mysocket.read_some(buff);
    while (l > 0) {
        tot.append(static_cast<const char*>(buff.data()), l);

        std::size_t pos = tot.find("\r");
        while (pos != std::string::npos) {
            std::string msg = tot.substr(0, pos);
            tot.erase(0, pos+1);
            auto c_string = msg.c_str();

            auto message = messages::IncomingMessageFromHost((c_string, c_string + msg.size()));
            static_cast<void>(tasks.comms->get_message_queue().try_send(message));
            pos = tot.find("\r");
        }
        l = mysocket.read_some(buff);
    }
}

sim_driver::StdinSimDriver::StdinSimDriver() { }
std::string sim_driver::StdinSimDriver::get_name() { return this-> name; }
void sim_driver::StdinSimDriver::write() { }
void sim_driver::StdinSimDriver::read(tasks::Tasks<SimulatorMessageQueue>& tasks) {
    auto linebuf = std::make_shared<std::string>(1024, 'c');
    while (true) {
        if (!std::cin.getline(linebuf->data(), linebuf->size() - 1, '\n')) {
            return;
        }
        auto wrote_to = std::cin.gcount();

        linebuf->at(wrote_to - 1) = '\n';

        auto message = messages::IncomingMessageFromHost(linebuf->data(), linebuf->data() + wrote_to);
        static_cast<void>(tasks.comms->get_message_queue().try_send(message));
    }
}
