/**
 * The ThermalPlatePolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

#include "firmware/thermal_hardware.h"
#include "thermocycler-refresh/thermal_general.hpp"

namespace plate_policy {

template <typename Iter>
concept ByteIterator = requires {
    {std::forward_iterator<Iter>};
    {std::is_same_v<std::iter_value_t<Iter>, uint8_t>};
};

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

    auto i2c_write(uint8_t addr, uint8_t data) -> bool;

    template <ByteIterator Input>
    auto i2c_write(uint8_t addr, Input data, size_t length) -> bool {
        return thermal_i2c_write_data(addr, &(*data), length);
    }

    template <ByteIterator Output>
    auto i2c_read(uint8_t addr, Output data, size_t length) -> bool {
        return thermal_i2c_read_data(addr, &(*data), length);
    }
};

}  // namespace plate_policy
