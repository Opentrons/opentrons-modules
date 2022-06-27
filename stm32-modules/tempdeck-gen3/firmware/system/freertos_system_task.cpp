#include "firmware/freertos_system_task.hpp"

#include "firmware/system_policy.hpp"
#include "tempdeck-gen3/system_task.hpp"

namespace system_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::SystemQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "System Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = system_task::SystemTask(_queue, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    _top_task.provide_aggregator(aggregator);
    auto policy = SystemPolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace system_control_task
