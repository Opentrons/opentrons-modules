#include "firmware/freertos_tasks.hpp"
#include "firmware/ui_hardware.h"

static constexpr uint32_t HALF_SECOND = 100;

namespace ui_control_task {

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    std::ignore = aggregator;
    while (true) {
        ui_hardware_set_heartbeat_led(true);
        vTaskDelay(HALF_SECOND);
        ui_hardware_set_heartbeat_led(false);
        vTaskDelay(HALF_SECOND);
    }
}

};  // namespace ui_control_task
