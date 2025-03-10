#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "systemwide.h"

namespace tof::hardware {
using namespace i2c::hardware;
class TOFSensorPolicy {
  public:
    explicit TOFSensorPolicy(I2CBase *i2c) : i2c_comms(i2c) {}
    TOFSensorPolicy(const TOFSensorPolicy &) = delete;
    TOFSensorPolicy(const TOFSensorPolicy &&) = delete;
    auto operator=(const TOFSensorPolicy &) = delete;
    auto operator=(const TOFSensorPolicy &&) = delete;
    ~TOFSensorPolicy() = default;

    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t size)
        -> RxTxReturn;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn;
    auto static enable_tof_sensor(TOFSensorID sensor_id, bool enable) -> void;
    auto static sleep_ms(uint32_t ms) -> void;

  private:
    I2CBase *i2c_comms{nullptr};
};
}  // namespace hardware::tof
