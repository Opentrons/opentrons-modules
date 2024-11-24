#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "flex-stacker/tof_driver_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace tof_driver_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::TOFDriverQueue
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "TOF Driver Queue");

static auto _top_task = tof_driver_task::TOFDriverTask(_queue, nullptr);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    //spi_hardware_init();

    //auto policy = motor_driver_policy::MotorDriverPolicy();
    while (true) {
        _top_task.run_once();
    }
}

};  // namespace tof_driver_task
