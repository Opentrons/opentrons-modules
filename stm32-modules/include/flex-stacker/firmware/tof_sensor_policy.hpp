#pragma once

#include <utility>

#include "firmware/tof_sensor_hardware.h"
#include "systemwide.h"

namespace tof_sensor_policy {

template <typename Iter>
concept ByteIterator = requires {
    {std::forward_iterator<Iter>};
    {std::is_same_v<std::iter_value_t<Iter>, uint8_t>};
};

class TOFSensorPolicy {
  public:
    TOFSensorPolicy() = default;

    auto set_enabled(TOFSensorID sensor_id, bool enabled) -> void;

    auto set_write_protect(TOFSensorID sensor_id, bool write_protect) -> void;

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

}  // namespace tof_sensor_policy
