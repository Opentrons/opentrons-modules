#pragma once

#include <cstdint>

namespace i2c {

namespace hardware {
/**
 * Abstract i2c interfacew.
 */
class I2CBase {
  public:
    I2CBase() = default;
    virtual ~I2CBase() = default;
    I2CBase(const I2CBase&) = default;
    auto operator=(const I2CBase&) -> I2CBase& = default;
    I2CBase(I2CBase&&) = default;
    auto operator=(I2CBase&&) -> I2CBase& = default;

    /**
     * Abstract transmit function
     * @return True if succeeded
     */
    virtual auto central_transmit(uint8_t* data, uint16_t size,
                                  uint16_t dev_address, uint32_t timeout)
        -> bool = 0;

    /**
     * Abstract receive function
     * @return True if succeeded
     */
    virtual auto central_receive(uint8_t* data, uint16_t size,
                                 uint16_t dev_address, uint32_t timeout)
        -> bool = 0;
};

};  // namespace hardware

};  // namespace i2c
