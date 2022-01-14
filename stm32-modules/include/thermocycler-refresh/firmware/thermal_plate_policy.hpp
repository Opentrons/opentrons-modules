/**
 * The ThermalPlatePolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

#include "thermocycler-refresh/thermal_general.hpp"

class ThermalPlatePolicy {
  public:
    ThermalPlatePolicy() = default;

    auto set_enabled(bool enabled) -> void;

    auto set_peltier(PeltierID peltier, double power,
                     PeltierDirection direction) -> bool;

    auto get_peltier(PeltierID peltier) -> std::pair<PeltierDirection, double>;

    auto set_fan(double power) -> bool;
};
