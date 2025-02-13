#pragma once

#include <cstdint>
#include <tuple>

using std::size_t;
static constexpr size_t MESSAGE_LEN = 5;
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

namespace i2c {
namespace hardware {
using RxTxReturn = std::tuple<uint8_t, MessageT>;
class I2CBase {
  public:
    I2CBase() = default;
    virtual ~I2CBase() = default;
    I2CBase(const I2CBase&) = default;
    auto operator=(const I2CBase&) -> I2CBase& = default;
    I2CBase(I2CBase&&) = default;
    auto operator=(I2CBase&&) -> I2CBase& = default;

    virtual auto i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size)
        -> RxTxReturn;
    virtual auto i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                           uint16_t size) -> RxTxReturn;
};

};  // namespace hardware

};  // namespace i2c
