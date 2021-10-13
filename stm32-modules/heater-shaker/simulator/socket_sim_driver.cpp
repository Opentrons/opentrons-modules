#include "simulator/socket_sim_driver.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <iterator>
#include <memory>
#include <regex>

#include "hal/double_buffer.hpp"
#include "simulator/simulator_queue.hpp"

using namespace socket_sim_driver;

const std::string SOCKET_DRIVER_NAME = "Socket";

std::unique_ptr<boost::asio::ip::tcp::socket> connect_to_socket(std::string host, int port) {
    boost::asio::io_service io_context;

    auto socket = std::make_unique<boost::asio::ip::tcp::socket>(io_context);
    boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string(host), port);
    boost::system::error_code ec;
    socket->connect(
            endpoint,
            ec);
    if (ec) {
        std::cerr << "Failed to create socket: " << ec.category().name() << ": "
                  << ec.value() << std::endl;
        exit(ec.value());
    }
    return socket;
}

socket_sim_driver::SocketSimDriver::SocketSimDriver(std::string url) {
    std::regex url_regex(":\\/\\/([a-zA-Z0-9.]*):(\\d*)$");
    std::smatch url_match_result;

    if (std::regex_search(url, url_match_result, url_regex)) {
        address_info = socket_sim_driver::AddressInfo{
            url_match_result[1], std::stoi(url_match_result[2])};
        s = connect_to_socket(address_info.host, address_info.port);

    } else {
        std::cerr << "Malformed url." << std::endl;
        exit(1);
    }
}



const std::string socket_sim_driver::SocketSimDriver::name = SOCKET_DRIVER_NAME;

std::string socket_sim_driver::SocketSimDriver::get_host() {
    return this->address_info.host;
}

int socket_sim_driver::SocketSimDriver::get_port() {
    return this->address_info.port;
}

const std::string& socket_sim_driver::SocketSimDriver::get_name() const {
    return this->name;
}

void socket_sim_driver::SocketSimDriver::write(std::string message)  {
    this->s->write_some(boost::asio::buffer(message));
}

int has_char(char* char_array, char value_to_find) {
    char* position =
        std::find(char_array, char_array + strlen(char_array), value_to_find);
    return char_array + strlen(char_array) != position;
}

void socket_sim_driver::SocketSimDriver::read(
    tasks::Tasks<SimulatorMessageQueue>& tasks) {
    char pBuff[30];
    boost::asio::mutable_buffer buff(pBuff, sizeof(pBuff));
    auto write_buffer =
        std::make_shared<double_buffer::DoubleBuffer<char, 2048>>();
    size_t l = this->s->read_some(buff);
    char* end_of_input = std::begin(*write_buffer->accessible());
    char* end_of_buffer = std::end(*write_buffer->accessible());
    for (; l > 0; l = this->s->read_some(buff)) {
        if (end_of_input + l > end_of_buffer) {
            end_of_input = std::begin(*write_buffer->accessible());
        }
        char* data = write_buffer->accessible()->data();
        end_of_input =
            std::copy(reinterpret_cast<char*>(buff.data()),
                      reinterpret_cast<char*>(buff.data()) + l, end_of_input);
        if (has_char(data, '\n')) {
            auto message = messages::IncomingMessageFromHost(
                std::begin(*write_buffer->accessible()), end_of_input);
            static_cast<void>(
                tasks.comms->get_message_queue().try_send(message));
            write_buffer->swap();
            end_of_input = std::begin(*write_buffer->accessible());
            end_of_buffer = std::end(*write_buffer->accessible());
        }
    }
}
