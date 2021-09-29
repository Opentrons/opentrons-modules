#include <iostream>

#include "heater-shaker/tasks.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/heater_thread.hpp"
#include "simulator/motor_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/system_thread.hpp"
#include "simulator/cli_parser.hpp"
#include "simulator/sim_driver.hpp"


using namespace std;

int main(int argc, char *argv[]) {

    auto sim_driver = cli_parser::get_sim_driver(argc, argv);
    cout << sim_driver->get_name() << endl;
//
//    auto reader = socket_reader::SocketReader("localhost", 9999);
//    std::cout << reader.get_host() << std::endl;
//    std::cout << reader.get_port() << std::endl;



//    auto system = system_thread::build();
//    auto heater = heater_thread::build();
//    auto motor = motor_thread::build();
//    auto comms = comm_thread::build();
//    auto tasks = tasks::Tasks<SimulatorMessageQueue>(heater.task, comms.task,
//                                                     motor.task, system.task);
//    comm_thread::handle_input(tasks);
//    system.handle->request_stop();
//    heater.handle->request_stop();
//    motor.handle->request_stop();
//    comms.handle->request_stop();
//    system.handle->join();
//    heater.handle->join();
//    motor.handle->join();
//    comms.handle->join();
//    return 0;
}
