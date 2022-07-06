#pragma once

struct TestThermistorPolicy {
    [[nodiscard]] auto get_time_ms() const -> uint32_t { return _time_ms; }

    // For test integration to bump up time
    auto advance_time(uint32_t time_ms) { _time_ms += time_ms; }

    uint32_t _time_ms = 0;
};
