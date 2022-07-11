#include "firmware/freertos_thermistor_task.hpp"

#include "FreeRTOS.h"
#include "firmware/thermistor_hardware.h"
#include "firmware/thermistor_policy.hpp"
#include "task.h"
#include "tempdeck-gen3/thermal_task.hpp"
#include "tempdeck-gen3/thermistor_task.hpp"

namespace thermistor_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task =
    thermistor_task::ThermistorTask<FreeRTOSMessageQueue>(nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static_assert(configTICK_RATE_HZ == 1000,
                  "FreeRTOS tickrate must be at 1000 Hz");

    thermistor_hardware_init();

    // Thermistor task has no queue, just need to provide aggregator handle
    _top_task.provide_aggregator(aggregator);

    auto policy = ThermistorPolicy();
    auto last_wake_time = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake_time,
                        decltype(_top_task)::THERMISTOR_READ_PERIOD_MS);
        _top_task.run_once(policy);
    }
}

};  // namespace thermistor_control_task
