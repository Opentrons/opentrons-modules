
#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "flex-stacker/tof_sensor_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace tof_sensor_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::TOFSensorQueue _queue(
    static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "TOF Sensor Queue");

static auto _top_task = tof_sensor_task::TOFSensorTask(_queue, nullptr);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    while (true) {
        _top_task.run_once();
    }
}

};     // namespace tof_sensor_task
    ;  // namespace tof_driver_task
