#include "firmware/pressure_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/pressure_sensor_hardware.h"
#include "firmware/vent_hardware.h"
#include "projdefs.h"
#include "systemwide.h"
#include "task.h"

using namespace pressure_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PressurePolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto PressurePolicy::get_time_ms() const -> uint32_t {
    return xTaskGetTickCount();
}

auto PressurePolicy::conversion_ended(PressureSensorID sensor_id) -> bool {
    return sensor_hardware_read_eoc_pin(sensor_id);
}

auto PressurePolicy::sensor_reset(PressureSensorID sensor_id) -> void {
    sensor_hardware_sensor_reset(sensor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PressurePolicy::set_vent_state(VentState state) -> void { hw_set_vent_state(state); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PressurePolicy::get_vent_state() -> VentState { return hw_get_vent_state(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PressurePolicy::get_vent_fault() -> bool {
    return hw_vent_fault_detected();
}

auto PressurePolicy::start_pressure_control(bool enable) -> void {
    auto *handle = static_cast<TaskHandle_t>(hardware_handle);
    if (handle != nullptr) {
        if (enable) {
            t_resync_needed->store(true);
            vTaskResume(handle);
        } else {
            vTaskSuspend(handle);
        }
    }
}
