#pragma once

#include <algorithm>

class TestLidHeaterPolicy {
  public:
    auto set_heater_power(double power) -> bool {
        _power = std::clamp(power, (double)0.0F, (double)1.0F);
        return true;
    }
    auto get_heater_power() const -> double { return _power; }

  private:
    double _power = 0.0F;
};
