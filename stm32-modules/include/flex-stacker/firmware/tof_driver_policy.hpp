#pragma once

#include <stdint.h>
#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "systemwide.h"

namespace tof {
namespace hardware {
using namespace i2c::hardware;
class TOFDriverPolicy {
  public:
    explicit TOFDriverPolicy(I2CBase *i2c) :
        i2c_comms(i2c) {}
    TOFDriverPolicy(const TOFDriverPolicy &) = delete;
    TOFDriverPolicy(const TOFDriverPolicy &&) = delete;
    auto operator=(const TOFDriverPolicy &) = delete;
    auto operator=(const TOFDriverPolicy &&) = delete;

    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size) -> RxTxReturn;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn;
    auto enable_tof_sensor(TOFSensorID sensor_id, bool enable) -> void;
    auto sleep_ms(uint32_t ms) -> void;

  private:
    I2CBase *i2c_comms{nullptr};
};
};  // namespace hardware
};  // namespace tof
