#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/tof_sensor_hardware.h"
#include "firmware/tof_sensor_policy.hpp"
#include "flex-stacker/tof_sensor_task.hpp"
#include "task.h"

namespace tof_sensor_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::TOFSensorQueue _queue(
    static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "TOF Sensor Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task =
    tof_sensor_task::TOFSensorTask(_queue, nullptr, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_comm) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    tof_hardware_init();
    auto policy = TOFSensorPolicy(i2c_comm);
    while (true) {
        _top_task.run_once(&policy);
    }
}

}  // namespace tof_sensor_task
