#include "firmware/pump_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/pump_hardware.h"
#include "task.h"

using namespace pump_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::start_pump_motor() -> bool {
    return hw_start_pump_motor();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::stop_pump_motor() -> bool {
    return hw_stop_pump_motor();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::set_pump_duty_cycle(uint16_t duty) -> void {
    hw_set_pump_duty_cycle(duty);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::get_pump_duty_cycle() -> uint16_t {
    return hw_get_pump_duty_cycle();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::enable_pump_tach(bool enable) -> bool {
    return hw_enable_pump_tach(enable);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto pump_policy::PumpPolicy::get_pump_rpm() -> float {
    return hw_get_pump_rpm();
}

auto pump_policy::PumpPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

auto pump_policy::PumpPolicy::enable_pump_control(bool enable) -> void {
    auto handle = static_cast<TaskHandle_t>(hardware_handle);
    if (enable) {
        vTaskResume(handle);
    } else {
        vTaskSuspend(handle);
    }
}
