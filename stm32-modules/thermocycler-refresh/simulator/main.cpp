#include <iostream>
#include <memory>

#include "simulator/cli_parser.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/system_thread.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    auto sim_driver = cli_parser::get_sim_driver(argc, argv);
    auto system = system_thread::build();
    auto comms = comm_thread::build(std::move(sim_driver));
    auto tasks = tasks::Tasks<SimulatorMessageQueue>(comms.task, system.task);

    comm_thread::handle_input(std::move(sim_driver), tasks);

    system.handle->request_stop();
    comms.handle->request_stop();

    system.handle->join();
    comms.handle->join();

    return 0;
}
