#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "firmware/i2c_hardware.h"
#include "systemwide.h"

namespace i2c::hardware {
class I2C : public I2CBase {
  public:
    explicit I2C() = default;
    ~I2C() final = default;
    I2C(const I2C &) = delete;
    I2C(const I2C &&) = delete;
    auto operator=(const I2C &) = delete;
    auto operator=(const I2C &&) = delete;

    auto sleep_ms(uint32_t ms) -> void;
    auto set_handle(HAL_I2C_HANDLE i2c_handle, I2C_BUS bus) -> void;
    auto i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data, uint16_t size)
        -> RxTxReturn final;
    auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                   uint16_t size) -> RxTxReturn final;
    auto i2c_master_write(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> RxTxReturn final;
    auto i2c_master_read(uint16_t dev_addr, uint8_t *data, uint16_t size)
        -> RxTxReturn final;

  private:
    I2C_BUS bus = NO_BUS;
    HAL_I2C_HANDLE handle = nullptr;
};
}  // namespace i2c::hardware
