#pragma once

#include <cstdint>

class ThermalPolicy {
  public:
    auto enable_peltier() -> void;

    auto disable_peltier() -> void;

    auto set_peltier_heat_power(double power) -> bool;

    auto set_peltier_cool_power(double power) -> bool;
};
