#pragma once

#include <cstdint>

class ThermistorPolicy {
  public:
    [[nodiscard]] auto get_time_ms() const -> uint32_t;
};
