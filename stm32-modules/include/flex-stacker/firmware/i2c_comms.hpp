#pragma once

#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <optional>

#include "firmware/hardware_iface.hpp"
#include "firmware/i2c.h"
#include "systemwide.h"

// TODO: MOVE THIS ELSEWHERE;

using std::size_t;
static constexpr size_t MESSAGE_LEN = 5;
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

//

namespace i2c {
namespace hardware {
using RxTxReturn = std::optional<MessageT>;
class I2C : public I2CBase {
  public:
    explicit I2C() = default;
    ~I2C() final = default;
    I2C(const I2C &) = delete;
    I2C(const I2C &&) = delete;
    auto operator=(const I2C &) = delete;
    auto operator=(const I2C &&) = delete;

    auto transmit_receive(uint16_t dev_address, MessageT &data, bool read)
        -> RxTxReturn;
    auto set_handle(HAL_I2C_HANDLE i2c_handle) -> void;

  private:
    auto central_transmit(uint8_t *data, uint16_t size, uint16_t dev_address,
                          uint32_t timeout) -> uint8_t final;

    auto central_receive(uint8_t *data, uint16_t size, uint16_t dev_address,
                         uint32_t timeout) -> uint8_t final;

    HAL_I2C_HANDLE handle = nullptr;

    // Timeout in ms
    static constexpr auto TIMEOUT = 1000;
};
};  // namespace hardware
};  // namespace i2c
