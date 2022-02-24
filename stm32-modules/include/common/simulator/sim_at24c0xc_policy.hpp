#pragma once

#include <array>

#include "core/at24c0xc.hpp"

template <size_t PAGES>
class SimAT24C0XCPolicy {
  public:
    static constexpr const size_t PAGE_LENGTH = at24c0xc::PAGE_LENGTH;
    using Buffer = std::array<uint8_t, PAGES * PAGE_LENGTH>;

    SimAT24C0XCPolicy()
        : _buffer({0}), _data_pointer(0), _write_protect(true) {}

    // --- Policy fulfillment -----------

    template <size_t Length>
    auto i2c_write(uint8_t addr, std::array<uint8_t, Length> &data) -> bool {
        // Ignore address for test purposes
        static_cast<void>(addr);
        if (data.size() > 0) {
            if (data.at(0) >= _buffer.size()) {
                // Out of bounds write attempt
                return false;
            }
            _data_pointer = data.at(0);
            for (auto itr = data.begin() + 1; itr != data.end(); itr++) {
                if (!_write_protect) {
                    _buffer[_data_pointer++] = *itr;
                }
                // If data pointer is at a page boundary, wraparound
                if ((_data_pointer % PAGE_LENGTH) == 0) {
                    _data_pointer -= PAGE_LENGTH;
                }
            }
        }

        return true;
    }

    // When only writing 1 byte, this policy can just
    // update the internal data pointer
    auto i2c_write(uint8_t addr, uint8_t data_addr) {
        // Ignore address for test purposes
        static_cast<void>(addr);
        if (data_addr < _buffer.size()) {
            _data_pointer = data_addr;
            return true;
        }
        return false;
    }

    template <size_t Length>
    auto i2c_read(uint8_t addr, std::array<uint8_t, Length> &data) -> bool {
        // Ignore address for test purposes
        static_cast<void>(addr);
        // Data pointer is held over from the last transaction
        for (auto &itr : data) {
            itr = _buffer[_data_pointer++];
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
