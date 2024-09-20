#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_driver_policy.hpp"
#include "firmware/motor_hardware.h"
#include "flex-stacker/motor_driver_task.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "stm32g4xx_it.h"


namespace motor_driver_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorDriverQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Driver Queue");

[[nodiscard]] static auto motor_driver_callback_glue(void) {
    if (spi_stream()) {
        static_cast<void>(_queue.try_send_from_isr(
            messages::StallGuardResult{.value = 1}));
    }
}


// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_driver_task::MotorDriverTask(_queue, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    spi_hardware_init();
    initialize_stallguard_callback(motor_driver_callback_glue);

    auto policy = motor_driver_policy::MotorDriverPolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace motor_driver_task
