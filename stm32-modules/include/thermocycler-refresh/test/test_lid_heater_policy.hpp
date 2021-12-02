#pragma once

#include <algorithm>

class TestLidHeaterPolicy {
  public:
    double _power = 0.0F;
    auto set_heater_power(double power) -> bool {
        _power = std::clamp(power, (double)0.0F, (double)1.0F);
        return true;
    }
};
