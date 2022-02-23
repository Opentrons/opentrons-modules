/**
 * The ThermalPlatePolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

#include "firmware/thermal_hardware.h"
#include "thermocycler-refresh/thermal_general.hpp"

class ThermalPlatePolicy {
  public:
    ThermalPlatePolicy() = default;

    auto set_enabled(bool enabled) -> void;

    auto set_peltier(PeltierID peltier, double power,
                     PeltierDirection direction) -> bool;

    auto get_peltier(PeltierID peltier) -> std::pair<PeltierDirection, double>;

    auto set_fan(double power) -> bool;

    auto get_fan() -> double;

    auto set_write_protect(bool write_protect) -> void;

    template <size_t Length>
    auto i2c_write(uint8_t addr, std::array<uint8_t, Length> &data) -> bool {
        return thermal_i2c_write_data(addr, data.data(), Length);
    }

    auto i2c_write(uint8_t addr, uint8_t data) -> bool {
        return thermal_i2c_write_data(addr, &data, 1);
    }

    template <size_t Length>
    auto i2c_read(uint8_t addr, std::array<uint8_t, Length> &data) -> bool {
        return thermal_i2c_read_data(addr, data.data(), Length);
    }
};
