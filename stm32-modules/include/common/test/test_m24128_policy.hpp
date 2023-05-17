#pragma once

#include <array>
#include <iterator>

#include "core/m24128.hpp"

namespace m24128_test_policy {

template <typename Iter>
concept ByteIterator = requires {
    {std::forward_iterator<Iter>};
    {std::is_same_v<std::iter_value_t<Iter>, uint8_t>};
};

class TestM24128Policy {
  public:
    static constexpr const size_t PAGE_LENGTH = m24128::PAGE_LENGTH;
    using Buffer = std::array<uint8_t, 128 * PAGE_LENGTH>;

    TestM24128Policy() : _buffer(), _data_pointer(0), _write_protect(true) {
        for (size_t i = 0; i < _buffer.size(); ++i) {
            _buffer[i] = 0;
        }
    }

    // --- Policy fulfillment -----------

    template <ByteIterator Input>
    auto i2c_write(uint8_t addr, Input data, size_t len) -> bool {
        // Ignore address for test purposes
        static_cast<void>(addr);

        if (len >= 2) {
            _data_pointer = (*data) << 8;
            ++data;
            _data_pointer |= (*data);
            ++data;
            len -= 2;
        } else {
            return true;
        }

        if (_data_pointer >= _buffer.size()) {
            // Out of bounds write attempt
            return false;
        }

        while (!_write_protect && len > 0) {
            _buffer[_data_pointer++] = *data;
            ++data;
            len--;
            // If data pointer is at a page boundary, wraparound
            if ((_data_pointer % PAGE_LENGTH) == 0) {
                _data_pointer -= PAGE_LENGTH;
            }
        }

        return true;
    }

    template <ByteIterator Input>
    auto i2c_read(uint8_t addr, Input data, size_t len) -> bool {
        // Ignore address for test purposes
        static_cast<void>(addr);
        // Data pointer is held over from the last transaction
        for (size_t i = 0; i < len; ++i, ++data) {
            *data = _buffer[_data_pointer++];
            // Wraparound is at memory limit instead of page boundary
            if (_data_pointer == _buffer.size()) {
                _data_pointer = 0;
            }
        }
        return true;
    }

    auto set_write_protect(bool write_protect) -> void {
        _write_protect = write_protect;
    }

    Buffer _buffer;
    size_t _data_pointer;
    bool _write_protect;
};

}  // namespace m24128_test_policy
