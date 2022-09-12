#pragma once

#include <cstdint>
#include <iterator>

#include "firmware/thermistor_hardware.h"

namespace thermal_policy {

template <typename Iter>
concept ByteIterator = requires {
    {std::forward_iterator<Iter>};
    {std::is_same_v<std::iter_value_t<Iter>, uint8_t>};
};

class ThermalPolicy {
  public:
    auto enable_peltier() -> void;

    auto disable_peltier() -> void;

    auto set_peltier_heat_power(double power) -> bool;

    auto set_peltier_cool_power(double power) -> bool;

    auto set_fan_power(double power) -> bool;

    auto get_fan_rpm() const -> double;

    auto set_write_protect(bool set) -> void;

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

}  // namespace thermal_policy
