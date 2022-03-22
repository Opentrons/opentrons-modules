#include <iostream>
#include <memory>

#include "simulator/cli_parser.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/lid_heater_thread.hpp"
#include "simulator/motor_thread.hpp"
#include "simulator/periodic_data_thread.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/system_thread.hpp"
#include "simulator/thermal_plate_thread.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    auto cli_ret = cli_parser::get_sim_driver(argc, argv);
    auto sim_driver = cli_ret.first;
    auto realtime = cli_ret.second;

    auto periodic_data = periodic_data_thread::build(realtime);

    auto system = system_thread::build();
    auto thermal_plate = thermal_plate_thread::build(periodic_data.task);
    auto lid_heater = lid_heater_thread::build(periodic_data.task);
    auto motor = motor_thread::build();
    auto comms = comm_thread::build(std::move(sim_driver));
    auto tasks = tasks::Tasks<SimulatorMessageQueue>(
        comms.task, system.task, thermal_plate.task, lid_heater.task,
        motor.task);

    periodic_data.task->provide_tasks(&tasks);

    comm_thread::handle_input(std::move(sim_driver), tasks);

    system.handle->request_stop();
    comms.handle->request_stop();
    thermal_plate.handle->request_stop();
    lid_heater.handle->request_stop();
    motor.handle->request_stop();
    periodic_data.handle->request_stop();

    system.handle->join();
    comms.handle->join();
    thermal_plate.handle->join();
    lid_heater.handle->join();
    motor.handle->join();
    periodic_data.handle->join();

    return 0;
}
