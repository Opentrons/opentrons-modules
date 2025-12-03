#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "systemwide.h"

namespace vacuum_pressure_sensor::hardware {
using namespace i2c::hardware;
class VacuumPressureSensorPolicy {
  public:
    explicit VacuumPressureSensorPolicy(I2CBase *i2c) : i2c_comms(i2c) {}
    VacuumPressureSensorPolicy(const VacuumPressureSensorPolicy &) = delete;
    VacuumPressureSensorPolicy(const VacuumPressureSensorPolicy &&) = delete;
    auto operator=(const VacuumPressureSensorPolicy &) = delete;
    auto operator=(const VacuumPressureSensorPolicy &&) = delete;
    ~VacuumPressureSensorPolicy() = default;

    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t size)
        -> RxTxReturn;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn;
    auto i2c_master_write(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> RxTxReturn;
    auto i2c_master_read(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> RxTxReturn;
    auto static conversion_ended(VacuumPressureSensorId sensor_id) -> bool;
    auto static sensor_reset(VacuumPressureSensorId sensor_id) -> void;
    auto static sleep_ms(uint32_t ms) -> void;

  private:
    I2CBase *i2c_comms{nullptr};
};
}  // namespace vacuum_pressure_sensor::hardware