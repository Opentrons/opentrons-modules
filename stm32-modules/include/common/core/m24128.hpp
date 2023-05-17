/**
 * @file m24128.hpp
 * @brief Implements a generic driver for M24128 EEPROM IC
 *
 */

#pragma once

#include <array>
#include <concepts>
#include <cstring>

#include "core/bit_utils.hpp"

namespace m24128 {

static constexpr const size_t PAGE_LENGTH = 64;

template <typename Policy>
concept M24128_Policy = requires(Policy &policy, uint8_t addr,
                                 std::array<uint8_t, PAGE_LENGTH> array) {
    // Function to write a page.
    // Accepts I2C address, uint8_t iterator, and a length
    {
        policy.i2c_write(addr, array.begin(), array.size())
        } -> std::same_as<bool>;
    // Function to read a page (8 bytes).
    // Accepts I2C address, uint8_t iterator, and a length
    {
        policy.i2c_read(addr, array.begin(), array.size())
        } -> std::same_as<bool>;
    // Function to enable or disable protection.
    // True turns on protection, false disables it.
    { policy.set_write_protect(true) } -> std::same_as<void>;
};

template <uint8_t ADDRESS>
class M24128 {
  private:
    static constexpr size_t ADDRESS_BYTES = 2;
    // I2C address of the EEPROM, shifted 1 bit left from the
    // datasheet.
    static constexpr const uint8_t _address = ADDRESS << 1;

  public:
    static const constexpr uint16_t PAGES = 128;

    template <typename T, M24128_Policy Policy>
    requires std::is_trivially_copyable_v<T>
    auto write_value(uint8_t page, T value, Policy &policy) -> bool {
        // The type to be written must be serializable in a single buffer
        static_assert(sizeof(T) <= PAGE_LENGTH, "Type T must be max 64 bytes.");
        constexpr size_t Length = sizeof(T) + ADDRESS_BYTES;

        if (!populate_address(page)) {
            return false;
        }
        // Because T must be trivially copyable, this is not a dangerous copy
        std::memcpy(&(_buffer[ADDRESS_BYTES]), &value, sizeof(value));

        policy.set_write_protect(false);
        auto ret = policy.i2c_write(_address, _buffer.begin(), Length);
        policy.set_write_protect(true);

        return ret;
    }

    template <typename T, M24128_Policy Policy>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read_value(uint8_t page, Policy &policy)
        -> std::optional<T> {
        using RT = std::optional<T>;

        static_assert(sizeof(T) <= PAGE_LENGTH, "Type T must be max 64 bytes.");

        if (!populate_address(page)) {
            return std::nullopt;
        }

        // Must write the address before reading everything else
        if (!policy.i2c_write(_address, _buffer.begin(), ADDRESS_BYTES)) {
            return std::nullopt;
        }

        if (!policy.i2c_read(_address, _buffer.begin(), PAGE_LENGTH)) {
            return std::nullopt;
        }
        T value;
        memcpy(&value, &_buffer[0], sizeof(value));
        return RT(value);
    }

  private:
    auto populate_address(uint8_t page) -> bool {
        if (page > PAGES) {
            return false;
        }
        uint16_t start_addr = page * PAGE_LENGTH;
        // MSB then LSB
        _buffer.at(0) = static_cast<uint8_t>((start_addr & 0xFF00) >> 8);
        _buffer.at(1) = static_cast<uint8_t>((start_addr)&0xFF);
        return true;
    }

    using Buffer = std::array<uint8_t, PAGE_LENGTH + 2>;
    Buffer _buffer;  // Keep a static buffer to avoid reallocating on the stack
};

}  // namespace m24128