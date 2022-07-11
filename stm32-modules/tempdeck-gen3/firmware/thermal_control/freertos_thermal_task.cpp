#include "firmware/freertos_thermal_task.hpp"

#include "tempdeck-gen3/thermal_task.hpp"

namespace thermal_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::ThermalQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Thermal Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = thermal_task::ThermalTask(_queue, nullptr);

struct FakePolicy {
    auto operator()() -> void {}
};
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    auto policy = FakePolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace thermal_control_task