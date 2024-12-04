#pragma once

#include <stdint.h>
#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "firmware/i2c_hardware.h"
#include "systemwide.h"

namespace i2c {
namespace hardware {
class I2C : public I2CBase {
  public:
    explicit I2C() = default;
    ~I2C() final = default;
    I2C(const I2C &) = delete;
    I2C(const I2C &&) = delete;
    auto operator=(const I2C &) = delete;
    auto operator=(const I2C &&) = delete;

    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size) -> RxTxReturn;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn;
    auto set_handle(HAL_I2C_HANDLE i2c_handle) -> void;
    auto enable_tof_sensor(TOFSensorID sensor_id, bool enable) -> void;

  private:
    HAL_I2C_HANDLE handle = nullptr;
    // Timeout in ms
    static constexpr auto TIMEOUT = 1000;
};
};  // namespace hardware
};  // namespace i2c
