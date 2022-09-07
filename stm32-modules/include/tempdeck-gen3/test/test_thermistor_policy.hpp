#pragma once

#include "test/test_ads1115_policy.hpp"

struct TestThermistorPolicy : public ads1115_test_policy::ADS1115TestPolicy {
    [[nodiscard]] auto get_time_ms() const -> uint32_t { return _time_ms; }
    auto sleep_ms(uint32_t time_ms) { _time_ms += time_ms; }
    auto get_imeas_adc_reading() -> uint32_t { return _imeas_adc_val; }

    uint32_t _time_ms = 0;
    uint32_t _imeas_adc_val = 0;
};
