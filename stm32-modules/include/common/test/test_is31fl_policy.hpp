#pragma once

#include <array>
#include <map>

namespace test_is31fl_policy {

class TestIS31FLPolicy {
  public:
    TestIS31FLPolicy() : backing() {}

    template <size_t Len>
    auto i2c_write(uint8_t device_address, uint8_t register_address,
                   std::array<uint8_t, Len> &data) -> bool {
        last_address = device_address;
        for (size_t i = 0; i < data.size(); ++i) {
            backing[register_address + i] = data[i];
        }
        return true;
    }

    auto check_register(size_t register_address) -> uint8_t {
        if (backing.contains(register_address)) {
            return backing[register_address];
        }
        return 0x00;
    }

    std::map<size_t, uint8_t> backing;
    uint8_t last_address = 0;
};

};  // namespace test_is31fl_policy