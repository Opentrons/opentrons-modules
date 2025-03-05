#include <cstdint>
#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/ui_hardware.h"
#include "firmware/ui_policy.hpp"
#include "flex-stacker/ui_task.hpp"

namespace ui_control_task {
using namespace ui_policy;

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::UIQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "UI Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = ui_task::UITask(_queue, nullptr, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_comms) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    auto policy = UIPolicy(i2c_comms);
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace ui_control_task
