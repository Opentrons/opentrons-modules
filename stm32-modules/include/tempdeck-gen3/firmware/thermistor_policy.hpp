#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "firmware/i2c_hardware.h"
#include "ot_utils/freertos/freertos_synchronization.hpp"

class ThermistorPolicy {
  public:
    static constexpr uint8_t ADC_ADDRESS = (0x40) << 1;
    explicit ThermistorPolicy()
        : _initialized(false),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _mutex() {}

    [[nodiscard]] auto get_time_ms() const -> uint32_t;
    auto sleep_ms(uint32_t ms) -> void;
    auto ads1219_mark_initialized() -> void;
    auto ads1219_check_initialized() -> bool;
    auto ads1219_get_lock() -> void;
    auto ads1219_release_lock() -> void;
    auto ads1219_arm_for_read() -> bool;

    template <size_t N>
    auto ads1219_i2c_send_data(std::array<uint8_t, N> &data) -> bool {
        return i2c_hardware_write_data(I2C_BUS_THERMAL, ADC_ADDRESS,
                                       data.data(), N);
    }

    template <size_t N>
    auto ads1219_i2c_read_data(std::array<uint8_t, N> &data) -> bool {
        return i2c_hardware_read_data(I2C_BUS_THERMAL, ADC_ADDRESS, data.data(),
                                      N);
    }
    auto ads1219_wait_for_pulse(uint32_t max_wait) -> bool;
    auto get_imeas_adc_reading() -> uint32_t;

  private:
    std::atomic_bool _initialized;
    ot_utils::freertos_synchronization::FreeRTOSMutex _mutex;
};
