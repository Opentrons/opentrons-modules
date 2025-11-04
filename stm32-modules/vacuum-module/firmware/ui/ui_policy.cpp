#include "firmware/ui_policy.hpp"

#include "FreeRTOS.h"
#include "firmware/pump_hardware.h"
#include "firmware/ui_hardware.h"
#include "task.h"

namespace ui_policy {

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto UIPolicy::set_heartbeat_led(bool value) -> void {
    ui_hardware_set_heartbeat_led(value);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto UIPolicy::get_pump_rpm() -> float { return hw_get_pump_rpm(); }

}  // namespace ui_policy
