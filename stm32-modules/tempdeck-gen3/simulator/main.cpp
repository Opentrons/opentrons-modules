#include "simulator/cli_parser.hpp"
#include "simulator/sim_driver.hpp"
#include "simulator/simulator_tasks.hpp"

auto main(int argc, char* argv[]) -> int {
    auto cli_ret = cli_parser::get_sim_driver(argc, argv);
    auto sim_driver = cli_ret.first;

    auto comms_queue = std::make_shared<tasks::SimTasks::HostCommsQueue>();
    auto system_queue = std::make_shared<tasks::SimTasks::SystemQueue>();
    auto ui_queue = std::make_shared<tasks::SimTasks::UIQueue>();
    auto thermal_queue = std::make_shared<tasks::SimTasks::ThermalQueue>();

    auto aggregator = std::make_shared<tasks::SimTasks::QueueAggregator>(
        *comms_queue, *system_queue, *ui_queue, *thermal_queue);

    auto comms = std::make_unique<std::jthread>(
        tasks::run_comms_task, comms_queue, aggregator, sim_driver);
    auto system = std::make_unique<std::jthread>(tasks::run_system_task,
                                                 system_queue, aggregator);
    auto ui = std::make_unique<std::jthread>(tasks::run_ui_task, ui_queue,
                                             aggregator);
    auto thermal = std::make_unique<std::jthread>(tasks::run_thermal_task,
                                                  thermal_queue, aggregator);
    auto thermistor =
        std::make_unique<std::jthread>(tasks::run_thermistor_task, aggregator);

    auto send_to_comms = [&aggregator](messages::IncomingMessageFromHost& msg) {
        aggregator->send(msg);
    };
    sim_driver->read(std::move(send_to_comms));

    // Previous line returns when connection is closed
    comms->request_stop();
    system->request_stop();
    ui->request_stop();
    thermal->request_stop();
    thermistor->request_stop();

    comms->join();
    system->join();
    ui->join();
    thermal->join();
    thermistor->join();

    return 0;
}
