#include <iostream>

#include "heater-shaker/tasks.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/heater_thread.hpp"
#include "simulator/motor_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/ui_thread.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    auto ui = ui_thread::build();
    auto heater = heater_thread::build();
    auto motor = motor_thread::build();
    auto comms = comm_thread::build();
    auto tasks = tasks::Tasks<SimulatorMessageQueue>(heater.task, comms.task,
                                                     motor.task, ui.task);
    comm_thread::handle_input(tasks);
    ui.handle->request_stop();
    heater.handle->request_stop();
    motor.handle->request_stop();
    comms.handle->request_stop();
    ui.handle->join();
    heater.handle->join();
    motor.handle->join();
    comms.handle->join();
    return 0;
}
