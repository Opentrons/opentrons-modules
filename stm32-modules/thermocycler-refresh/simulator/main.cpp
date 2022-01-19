#include <iostream>
#include <memory>

#include "simulator/cli_parser.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/lid_heater_thread.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/system_thread.hpp"
#include "simulator/thermal_plate_thread.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "simulator/motor_thread.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    auto sim_driver = cli_parser::get_sim_driver(argc, argv);
    auto system = system_thread::build();
    auto thermal_plate = thermal_plate_thread::build();
    auto lid_heater = lid_heater_thread::build();
    auto motor = motor_thread::build();
    auto comms = comm_thread::build(std::move(sim_driver));
    auto tasks = tasks::Tasks<SimulatorMessageQueue>(
        comms.task, system.task, thermal_plate.task, lid_heater.task, motor.task);

    comm_thread::handle_input(std::move(sim_driver), tasks);

    system.handle->request_stop();
    comms.handle->request_stop();
    thermal_plate.handle->request_stop();
    lid_heater.handle->request_stop();
    motor.handle->request_stop();

    system.handle->join();
    comms.handle->join();
    thermal_plate.handle->join();
    lid_heater.handle->join();
    motor.handle->join();

    return 0;
}
