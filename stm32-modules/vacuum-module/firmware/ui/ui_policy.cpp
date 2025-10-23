#include "firmware/ui_policy.hpp"

#include "FreeRTOS.h"
#include "firmware/ui_hardware.h"
#include "task.h"

namespace ui_policy {

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto UIPolicy::set_heartbeat_led(bool value) -> void {
    ui_hardware_set_heartbeat_led(value);
}

auto UIPolicy::sleep_ms(uint32_t ms) -> void { vTaskDelay(pdMS_TO_TICKS(ms)); }
}  // namespace ui_policy
