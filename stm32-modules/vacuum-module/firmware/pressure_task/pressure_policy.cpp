#include "firmware/pressure_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/pressure_sensor_hardware.h"
#include "projdefs.h"
#include "systemwide.h"
#include "task.h"

using namespace pressure_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PressurePolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

auto PressurePolicy::conversion_ended(PressureSensorID sensor_id) -> bool {
    return sensor_hardware_read_eoc_pin(sensor_id);
}

auto PressurePolicy::sensor_reset(PressureSensorID sensor_id) -> void {
    sensor_hardware_sensor_reset(sensor_id);
}
