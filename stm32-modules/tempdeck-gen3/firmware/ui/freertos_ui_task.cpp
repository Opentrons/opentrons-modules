#include "firmware/freertos_ui_task.hpp"

#include "firmware/ui_policy.hpp"
#include "tempdeck-gen3/ui_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "firmware/ui_hardware.h"

namespace ui_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::UIQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "UI Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = ui_task::UITask(_queue, nullptr);

static auto _timer_callback = [ObjectPtr = &_top_task] {ObjectPtr->update_callback();};

// This timer provides the backing timing for the UI task
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _ui_timer = ot_utils::freertos_timer::FreeRTOSTimer(
    "UI Timer", 
    _timer_callback,
    _top_task.UPDATE_PERIOD_MS);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    ui_hardware_initialize();

    auto policy = UIPolicy();
    _ui_timer.start();
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace ui_control_task
