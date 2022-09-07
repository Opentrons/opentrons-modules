#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "test/test_ads1115_policy.hpp"

struct SimThermistorPolicy {
    explicit SimThermistorPolicy(bool realtime = false)
        : _realtime(realtime), _written() {}

    [[nodiscard]] auto get_time_ms() const -> uint32_t { return _time_ms; }
    auto sleep_ms(uint32_t time_ms) {
        if (_realtime) {
            using namespace std::chrono_literals;
            auto time = 1ms * time_ms;
            std::this_thread::sleep_for(time);
        }
        _time_ms += time_ms;
    }

    auto ads1115_mark_initialized() -> void { _initialized = true; }

    auto ads1115_check_initialized() -> bool { return _initialized; }

    auto ads1115_get_lock() -> void {
        while (_locked) {
            std::this_thread::yield();
        }
        _locked = true;
    }

    auto ads1115_release_lock() -> void { _locked = false; }

    auto ads1115_arm_for_read() -> bool {
        _read_armed = true;
        return true;
    }

    auto ads1115_wait_for_pulse(uint32_t timeout_ms) -> bool {
        if (_read_armed) {
            _read_armed = false;
            return true;
        }
        return false;
    }

    auto ads1115_i2c_write_16(uint8_t reg, uint16_t val) -> bool {
        _written[reg] = val;
        return true;
    }

    auto ads1115_i2c_read_16(uint8_t reg) -> std::optional<uint16_t> {
        using Ret = std::optional<uint16_t>;
        if (_written.find(reg) != _written.end()) {
            return Ret(_written.at(reg));
        }
        return Ret(0);
    }

    auto get_imeas_adc_reading() -> uint32_t {
        // TODO - add simulator current feedback
        return 0;
    }

    uint32_t _time_ms = 0;

  private:
    bool _realtime;
    std::atomic_bool _initialized = false;
    std::atomic_bool _locked = false;
    std::atomic_bool _read_armed = false;
    // Written registers - addr : value
    std::map<uint8_t, uint16_t> _written;
};
