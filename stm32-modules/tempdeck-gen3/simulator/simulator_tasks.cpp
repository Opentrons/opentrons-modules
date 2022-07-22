#include "simulator/simulator_tasks.hpp"

#include "simulator/sim_system_policy.hpp"
#include "simulator/sim_thermal_policy.hpp"
#include "simulator/sim_thermistor_policy.hpp"
#include "simulator/sim_ui_policy.hpp"
#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/system_task.hpp"
#include "tempdeck-gen3/thermal_task.hpp"
#include "tempdeck-gen3/thermistor_task.hpp"
#include "tempdeck-gen3/ui_task.hpp"

using namespace tasks;

auto tasks::run_comms_task(
    std::stop_token st, std::shared_ptr<SimTasks::HostCommsQueue> queue_ptr,
    std::shared_ptr<SimTasks::QueueAggregator> aggregator,
    std::shared_ptr<sim_driver::SimDriver> driver) -> void {
    auto &queue = *queue_ptr;
    auto task = host_comms_task::HostCommsTask(queue, aggregator.get());
    std::string buffer(1024, 'c');

    queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            auto wrote_to = task.run_once(buffer.begin(), buffer.end());
            driver->write(std::string(buffer.begin(), wrote_to));
        } catch (const SimTasks::HostCommsQueue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto tasks::run_system_task(
    std::stop_token st, std::shared_ptr<SimTasks::SystemQueue> queue_ptr,
    std::shared_ptr<SimTasks::QueueAggregator> aggregator) -> void {
    auto &queue = *queue_ptr;
    auto policy = SimSystemPolicy();
    auto task = system_task::SystemTask(queue, aggregator.get());

    queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            task.run_once(policy);
        } catch (const SimTasks::SystemQueue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto tasks::run_ui_task(std::stop_token st,
                        std::shared_ptr<SimTasks::UIQueue> queue_ptr,
                        std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    auto &queue = *queue_ptr;
    auto policy = SimUIPolicy();
    auto task = ui_task::UITask(queue, aggregator.get());

    queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            task.run_once(policy);
        } catch (const SimTasks::UIQueue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto tasks::run_thermal_task(
    std::stop_token st, std::shared_ptr<SimTasks::ThermalQueue> queue_ptr,
    std::shared_ptr<SimTasks::QueueAggregator> aggregator) -> void {
    auto &queue = *queue_ptr;
    auto policy = SimThermalPolicy();
    auto task = thermal_task::ThermalTask(queue, aggregator.get());

    queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            task.run_once(policy);
        } catch (const SimTasks::ThermalQueue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto tasks::run_thermistor_task(
    std::stop_token st, std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    auto policy = SimThermistorPolicy();
    auto task = thermistor_task::ThermistorTask<SimulatorMessageQueue>(
        aggregator.get());

    while (!st.stop_requested()) {
        policy.sleep_ms(100);
        task.run_once(policy);
    }
}
