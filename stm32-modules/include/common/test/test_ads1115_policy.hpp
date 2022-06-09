#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>

namespace ads1115_test_policy {

struct ADS1115BackingData {
    bool initialized = false;
    bool locked = false;
    bool read_armed = false;
    // Number of times this has been locked & relocked
    size_t lock_count = 0;
    // Written registers - addr : value
    std::map<uint8_t, uint16_t> written;
};

struct ADS1115TestPolicy {
    static constexpr size_t ADS1115_COUNT = 2;
    // Reading a register always returns this
    static constexpr uint16_t READBACK_VALUE = 0xABCD;

    static constexpr std::array addresses = {(uint8_t)(0x48 << 1),
                                             (uint8_t)(0x49 << 1)};

    auto ads1115_mark_initialized(size_t id) -> void {
        _backing[id].initialized = true;
    }

    auto ads1115_check_initialized(size_t id) -> bool {
        return _backing[id].initialized;
    }

    auto ads1115_get_lock(size_t id) -> void {
        if (_backing[id].locked) {
            throw std::runtime_error("Can't wait on mutex in tests");
        }
        _backing[id].locked = true;
    }

    auto ads1115_release_lock(size_t id) -> void {
        if (_backing[id].locked) {
            _backing[id].lock_count++;
        }
        _backing[id].locked = false;
    }

    auto ads1115_arm_for_read(size_t id) -> bool {
        _backing[id].read_armed = true;
        return true;
    }

    auto ads1115_i2c_write_16(uint8_t addr, uint8_t reg, uint16_t val) -> bool {
        auto id = id_from_addr(addr);
        if (id == ADS1115_COUNT) {
            return false;
        }
        _backing[id].written[reg] = val;
        return true;
    }

    auto ads1115_i2c_read_16(uint8_t addr, uint8_t reg)
        -> std::optional<uint16_t> {
        auto id = id_from_addr(addr);
        if (id == ADS1115_COUNT) {
            return std::nullopt;
        }
        return std::optional<uint16_t>(READBACK_VALUE);
    }

    auto ads1115_wait_for_pulse(uint32_t timeout_ms) -> bool {
        bool ret = false;
        for (auto &channel : _backing) {
            if (channel.read_armed) {
                channel.read_armed = false;
                ret = true;
            }
        }
        return ret;
    }

    auto task_yield() -> void { return; }

    std::array<ADS1115BackingData, ADS1115_COUNT> _backing = {};

  private:
    auto id_from_addr(uint8_t addr) -> size_t {
        auto ret = std::find(addresses.begin(), addresses.end(), addr);
        if (ret == addresses.end()) {
            return ADS1115_COUNT;
        }
        return std::distance(addresses.begin(), ret);
    }
};

};  // namespace ads1115_test_policy
