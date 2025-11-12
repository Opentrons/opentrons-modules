#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace lps22df {


template <typename P>
concept LPS22DFPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

class LPS222DF {
  public:
    auto initialize(LPS22DFPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

};

}  // namespace lps22df
