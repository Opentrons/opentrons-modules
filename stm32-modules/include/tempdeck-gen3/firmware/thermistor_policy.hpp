#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "ot_utils/freertos/freertos_synchronization.hpp"

class ThermistorPolicy {
  public:
    static constexpr uint8_t ADC_ADDRESS = (0x48) << 1;
    explicit ThermistorPolicy()
        : _initialized(false),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _mutex() {}

    [[nodiscard]] auto get_time_ms() const -> uint32_t;
    auto sleep_ms(uint32_t ms) -> void;
    auto ads1115_mark_initialized() -> void;
    auto ads1115_check_initialized() -> bool;
    auto ads1115_get_lock() -> void;
    auto ads1115_release_lock() -> void;
    auto ads1115_arm_for_read() -> bool;
    auto ads1115_i2c_write_16(uint8_t reg, uint16_t data) -> bool;
    auto ads1115_i2c_read_16(uint8_t reg) -> std::optional<uint16_t>;
    auto ads1115_wait_for_pulse(uint32_t max_wait) -> bool;

  private:
    std::atomic_bool _initialized;
    ot_utils::freertos_synchronization::FreeRTOSMutex _mutex;
};
