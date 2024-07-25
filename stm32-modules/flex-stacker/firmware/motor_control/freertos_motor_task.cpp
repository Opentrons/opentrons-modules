#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/motor_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace motor_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_task::MotorTask(_queue, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    //        motor_hardware_init();

    //    auto policy = motor_policy::MotorPolicy();
    while (true) {
        //        _top_task.run_once(policy);
    }
}

};  // namespace motor_control_task
