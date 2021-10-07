#include "simulator/socket_sim_driver.hpp"

#include <boost/asio.hpp>
#include <regex>
#include <memory>
#include <algorithm>
#include <iterator>

#include "simulator/simulator_queue.hpp"
#include "hal/double_buffer.hpp"

using namespace socket_sim_driver;

const std::string SOCKET_DRIVER_NAME = "Socket";

socket_sim_driver::SocketSimDriver::SocketSimDriver(std::string url) {
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

const std::string socket_sim_driver::SocketSimDriver::name = SOCKET_DRIVER_NAME;

std::string socket_sim_driver::SocketSimDriver::get_host() {
    return this->host;
}

int socket_sim_driver::SocketSimDriver::get_port() { return this->port; }

const std::string& socket_sim_driver::SocketSimDriver::get_name() const {
    return this->name;
}

void socket_sim_driver::SocketSimDriver::write() {}

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

int get_index(char* char_array, char value_to_find) {
    char* position = std::find(char_array, char_array + strlen(char_array), value_to_find);
    return (char_array + strlen(char_array) == position) ? -1 : (position - char_array);
}

void socket_sim_driver::SocketSimDriver::read(
    tasks::Tasks<SimulatorMessageQueue>& tasks) {
    char pBuff[30];
    auto socket = get_socket(this->host, this->port);
    boost::asio::mutable_buffer buff(pBuff, sizeof(pBuff));
    auto write_buffer = std::make_shared<double_buffer::DoubleBuffer<char, 2048>>();

    size_t l = socket.read_some(buff);
    char* end_of_input = std::begin(*write_buffer->accessible());
    char* end_of_buffer = std::end(*write_buffer->accessible());
    for (; l > 0; l = socket.read_some(buff)) {
        if (end_of_input + l > end_of_buffer) {
            end_of_input = std::begin(*write_buffer->accessible());
        }
        char* data = write_buffer->accessible()->data();
        end_of_input = std::copy(reinterpret_cast<char*>(buff.data()), reinterpret_cast<char*>(buff.data()) + l, end_of_input);
        int pos = get_index(data, '\n');
        if ( pos != -1 ) {
            auto message = messages::IncomingMessageFromHost(std::begin(*write_buffer->accessible()), end_of_input);
            static_cast<void>(tasks.comms->get_message_queue().try_send(message));
            write_buffer->swap();
            end_of_input = std::begin(*write_buffer->accessible());
            end_of_buffer = std::end(*write_buffer->accessible());
        }
    }
}
