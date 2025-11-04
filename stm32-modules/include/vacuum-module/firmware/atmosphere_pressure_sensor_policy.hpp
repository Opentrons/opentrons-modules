#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "systemwide.h"

namespace atmosphere_pressure_sensor::hardware {
using namespace i2c::hardware;
class AtmospherePressureSensorPolicy {
  public:
    explicit AtmospherePressureSensorPolicy(I2CBase *i2c) : i2c_comms(i2c) {}
    AtmospherePressureSensorPolicy(const AtmospherePressureSensorPolicy &) =
        delete;
    AtmospherePressureSensorPolicy(const AtmospherePressureSensorPolicy &&) =
        delete;
    auto operator=(const AtmospherePressureSensorPolicy &) = delete;
    auto operator=(const AtmospherePressureSensorPolicy &&) = delete;
    ~AtmospherePressureSensorPolicy() = default;

    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t size)
        -> RxTxReturn;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn;
    auto static sleep_ms(uint32_t ms) -> void;

  private:
    I2CBase *i2c_comms{nullptr};
};
}  // namespace atmosphere_pressure_sensor::hardware