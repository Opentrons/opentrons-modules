#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_driver_policy.hpp"
#include "firmware/motor_hardware.h"
#include "flex-stacker/motor_driver_task.hpp"
#include "flex-stacker/tmc2160_interface.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace motor_driver_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorDriverQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Driver Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_driver_task::MotorDriverTask(_queue, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    spi_hardware_init();

    auto policy = motor_driver_policy::MotorDriverPolicy();
    auto tmc2160_interface = tmc2160::TMC2160Interface(policy);
    while (true) {
        _top_task.run_once(tmc2160_interface);
    }
}

};  // namespace motor_driver_task
