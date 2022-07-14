
#include "firmware/thermistor_policy.hpp"

#include "FreeRTOS.h"
#include "firmware/thermistor_hardware.h"
#include "semphr.h"
#include "task.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto ThermistorPolicy::get_time_ms() const -> uint32_t {
    return xTaskGetTickCount();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermistorPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

auto ThermistorPolicy::ads1115_mark_initialized() -> void {
    _initialized = true;
}

auto ThermistorPolicy::ads1115_check_initialized() -> bool {
    return _initialized;
}

auto ThermistorPolicy::ads1115_get_lock() -> void { _mutex.acquire(); }

auto ThermistorPolicy::ads1115_release_lock() -> void { _mutex.release(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermistorPolicy::ads1115_arm_for_read() -> bool {
    return thermal_arm_adc_for_read();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermistorPolicy::ads1115_i2c_write_16(uint8_t reg, uint16_t data)
    -> bool {
    return thermal_i2c_write_16(ADC_ADDRESS, reg, data);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermistorPolicy::ads1115_i2c_read_16(uint8_t reg)
    -> std::optional<uint16_t> {
    uint16_t data = 0;
    if (thermal_i2c_read_16(ADC_ADDRESS, reg, &data)) {
        return std::optional<uint16_t>(data);
    }
    return std::nullopt;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermistorPolicy::ads1115_wait_for_pulse(uint32_t max_wait) -> bool {
    auto notification_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(max_wait));
    return notification_val == 1;
}
