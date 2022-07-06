
#include "firmware/thermistor_policy.hpp"

#include "FreeRTOS.h"
#include "task.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto ThermistorPolicy::get_time_ms() const -> uint32_t {
    return xTaskGetTickCount();
}
