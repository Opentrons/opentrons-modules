#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>

namespace ads1115_test_policy {

struct ADS1115TestPolicy {
    // Reading a register always returns this
    static constexpr uint16_t READBACK_VALUE = 0xABCD;

    auto ads1115_mark_initialized() -> void {
        _initialized = true;
    }

    auto ads1115_check_initialized() -> bool {
        return _initialized;
    }

    auto ads1115_get_lock() -> void {
        if (_locked) {
            throw std::runtime_error("Can't wait on mutex in tests");
        }
        _locked = true;
    }

    auto ads1115_release_lock() -> void {
        if (_locked) {
            _lock_count++;
        }
        _locked = false;
    }

    auto ads1115_arm_for_read() -> bool {
        _read_armed = true;
        return true;
    }

    auto ads1115_i2c_write_16(uint8_t reg, uint16_t val) -> bool {
        _written[reg] = val;
        return true;
    }

    auto ads1115_i2c_read_16(uint8_t reg)
        -> std::optional<uint16_t> {
        return std::optional<uint16_t>(READBACK_VALUE);
    }

    auto ads1115_wait_for_pulse(uint32_t timeout_ms) -> bool {
        if (_read_armed) {
            _read_armed = false;
            return true;
        }
        return false;
    }

    auto task_yield() -> void { return; }

    bool _initialized = false;
    bool _locked = false;
    bool _read_armed = false;
    // Number of times this has been locked & relocked
    size_t _lock_count = 0;
    // Written registers - addr : value
    std::map<uint8_t, uint16_t> _written;
};

};  // namespace ads1115_test_policy
