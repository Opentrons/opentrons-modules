#include "simulator/sim_driver.hpp"

#include <boost/asio.hpp>
#include <regex>

#include "simulator/simulator_queue.hpp"

using namespace sim_driver;

sim_driver::SocketSimDriver::SocketSimDriver(std::string url) {
    std::regex url_regex(":\\/\\/([a-zA-Z0-9.]*):(\\d*)$");
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

std::string sim_driver::SocketSimDriver::get_name() { return this->name; }

void sim_driver::SocketSimDriver::write() {}

auto get_socket(std::string host, int port) {
    boost::asio::io_service io_context;

    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(host), port);
    boost::system::error_code ec;
    socket.connect(
        endpoint,
        ec);  // TODO: Change this to socket.bind - Derek Maggio 9/30/21
    if (ec) {
        std::cerr << "Failed to create socket: " << ec.category().name() << ": "
                  << ec.value() << std::endl;
        exit(ec.value());
    }
    return socket;
}

void sim_driver::SocketSimDriver::read(
    tasks::Tasks<SimulatorMessageQueue>& tasks) {
    char pBuff[30];
    auto socket = get_socket(this->host, this->port);
    auto linebuf = std::string(1024, 'c');
    boost::asio::mutable_buffer buff(pBuff, sizeof(pBuff));
    std::string read_data;

    /*
     * TODO: This algorithm starts dropping G-Codes if they are sent faster than
     * once every 0.005 seconds. Need to handle that Derek Maggio 9/30/21
     */
    std::size_t l = socket.read_some(buff);
    while (l > 0) {
        read_data.append(static_cast<const char*>(buff.data()), l);
        std::size_t pos = read_data.find("\n");

        while (pos != std::string::npos) {
            std::string msg = read_data.substr(0, pos + 1);
            linebuf.replace(0, msg.length(), msg);

            auto message = messages::IncomingMessageFromHost(
                linebuf.data(), linebuf.data() + msg.size());
            static_cast<void>(
                tasks.comms->get_message_queue().try_send(message));

            read_data.erase(0, pos + 1);
            pos = read_data.find("\n");
        }

        l = socket.read_some(buff);
    }
}

sim_driver::StdinSimDriver::StdinSimDriver() {}
std::string sim_driver::StdinSimDriver::get_name() { return this->name; }
void sim_driver::StdinSimDriver::write() {}
void sim_driver::StdinSimDriver::read(
    tasks::Tasks<SimulatorMessageQueue>& tasks) {
    auto linebuf = std::make_shared<std::string>(1024, 'c');
    while (true) {
        if (!std::cin.getline(linebuf->data(), linebuf->size() - 1, '\n')) {
            return;
        }
        auto wrote_to = std::cin.gcount();

        linebuf->at(wrote_to - 1) = '\n';
        auto message = messages::IncomingMessageFromHost(
            linebuf->data(), linebuf->data() + wrote_to);
        static_cast<void>(tasks.comms->get_message_queue().try_send(message));
    }
}
