#include "firmware/control_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "projdefs.h"
#include "task.h"

using namespace control_policy;


// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ControlPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
