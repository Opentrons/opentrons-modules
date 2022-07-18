/**
 * @file firmware_tasks.hpp
 * @brief Expands on generic tasks.hpp to provide specific typedefs
 * for the firmware build
 */
#pragma once

#include <memory>
#include <thread>

#include "simulator/sim_driver.hpp"
#include "simulator/simulator_queue.hpp"
#include "tempdeck-gen3/tasks.hpp"

namespace tasks {

using SimTasks = Tasks<SimulatorMessageQueue>;

auto run_comms_task(std::stop_token st,
                    std::shared_ptr<SimTasks::QueueAggregator> aggregator,
                    std::shared_ptr<sim_driver::SimDriver> driver) -> void;
auto run_system_task(std::stop_token st,
                     std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void;
auto run_ui_task(std::stop_token st,
                 std::shared_ptr<SimTasks::QueueAggregator> aggregator) -> void;
auto run_thermal_task(std::stop_token st,
                      std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void;
auto run_thermistor_task(std::stop_token st,
                         std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void;

};  // namespace tasks
