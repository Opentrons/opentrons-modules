/**
 * The LidHeaterPolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

class LidHeaterPolicy {
  public:
    LidHeaterPolicy() = default;

    auto set_heater_power(double power) -> bool;

    auto get_heater_power() const -> double;
};
