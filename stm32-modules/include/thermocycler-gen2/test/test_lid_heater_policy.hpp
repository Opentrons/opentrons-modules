#pragma once

#include <algorithm>

class TestLidHeaterPolicy {
  public:
    auto set_heater_power(double power) -> bool {
        _power = std::clamp(power, (double)0.0F, (double)1.0F);
        return true;
    }
    auto get_heater_power() const -> double { return _power; }

    auto set_lid_fans(bool enable) -> void { _lid_fans = enable; }

    auto lid_fans_enabled() const -> bool { return _lid_fans; }

  private:
    double _power = 0.0F;
    bool _lid_fans = false;
};
