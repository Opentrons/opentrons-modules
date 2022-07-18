#include "simulator/simulator_tasks.hpp"

#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/system_task.hpp"
#include "tempdeck-gen3/thermal_task.hpp"
#include "tempdeck-gen3/thermistor_task.hpp"
#include "tempdeck-gen3/ui_task.hpp"

using namespace tasks;

auto tasks::run_comms_task(
    std::stop_token st, std::shared_ptr<SimTasks::QueueAggregator> aggregator,
    std::shared_ptr<sim_driver::SimDriver> driver) -> void {
    auto queue = SimTasks::HostCommsQueue();
    aggregator->register_queue(queue);
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
    std::stop_token st, std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    while (!st.stop_requested()) {
    }
}

auto tasks::run_ui_task(std::stop_token st,
                        std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    while (!st.stop_requested()) {
    }
}

auto tasks::run_thermal_task(
    std::stop_token st, std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    while (!st.stop_requested()) {
    }
}

auto tasks::run_thermistor_task(
    std::stop_token st, std::shared_ptr<SimTasks::QueueAggregator> aggregator)
    -> void {
    while (!st.stop_requested()) {
    }
}
