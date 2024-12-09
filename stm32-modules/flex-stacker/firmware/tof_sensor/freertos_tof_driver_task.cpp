#include "FreeRTOS.h"

#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/tof_driver_policy.hpp"
#include "firmware/tof_sensor_hardware.h"
#include "flex-stacker/tof_driver_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace tof_driver_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::TOFDriverQueue _queue(
    static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "TOF Driver Queue");

static auto _top_task =
    tof_driver_task::TOFDriverTask(_queue, nullptr, nullptr);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_comm) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    auto policy = TOFDriverPolicy(i2c_comm);
    while (true) {
        _top_task.run_once(&policy);
    }
}

};  // namespace tof_driver_task
