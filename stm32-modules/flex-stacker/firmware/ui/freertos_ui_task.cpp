#include "firmware/freertos_tasks.hpp"

#include "firmware/ui_hardware.h"

namespace ui_control_task {

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    while (true) {
        ui_hardware_set_heartbeat_led(true);
        vTaskDelay(500);
        ui_hardware_set_heartbeat_led(false);
        vTaskDelay(500);
    }
}

};  // namespace ui_control_task
