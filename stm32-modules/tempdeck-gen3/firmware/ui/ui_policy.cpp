#include "firmware/ui_policy.hpp"

#include "firmware/ui_hardware.h"

auto UIPolicy::set_heartbeat_led(bool value) -> void {
    ui_hardware_set_heartbeat_led(value);
}
