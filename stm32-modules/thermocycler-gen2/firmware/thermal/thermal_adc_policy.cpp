
#include "firmware/thermal_adc_policy.hpp"

using namespace thermal_adc_policy;

static auto thermal_adc_policy::get_adc_1() -> ADCPolicy& {
    static ADCPolicy adc1_instance(AdcPolicy::ADC_1_ADDRESS,
                                   ADC_ITR_T::ADC1_ITR);
    return adc1_instance;
}

static auto thermal_adc_policy::get_adc_2() -> ADCPolicy& {
    static ADCPolicy adc2_instance(AdcPolicy::ADC_2_ADDRESS,
                                   ADC_ITR_T::ADC2_ITR);
    return adc2_instance;
}

auto AdcPolicy::ads1115_mark_initialized() -> void { _initialized = true; }

auto AdcPolicy::ads1115_check_initialized() -> bool { return _initialized; }

auto AdcPolicy::ads1115_get_lock() -> void { return _mutex.acquire(); }

auto AdcPolicy::ads1115_release_lock() -> void { return _mutex.release(); }

auto AdcPolicy::ads1115_arm_for_read() -> bool {
    return thermal_arm_adc_for_read(_id);
}

auto AdcPolicy::ads1115_i2c_write_16(uint8_t reg, uint16_t value) -> bool {
    return thermal_i2c_write_16(_i2c_address, reg, value);
}

auto AdcPolicy::ads1115_i2c_read_16(uint8_t reg) -> std::optional<uint16_t> {
    uint16_t ret;
    if (thermal_i2c_read_16(_i2c_address, reg, &ret)) {
        return std::optional<uint16_t>(ret);
    }
    return std::nullopt;
}

auto AdcPolicy::ads1115_wait_for_pulse(uint32_t max_wait_ms) -> bool {
    auto notification_val =
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(max_wait_ms));
    return notification_val == 1;
}

auto AdcPolicy::task_yield() -> void { taskYIELD(); }

AdcPolicy::AdcPolicy(uint8_t address, size_t id)
    : _i2c_address(address), _id(id) _initialized(false), _mutex() {}
