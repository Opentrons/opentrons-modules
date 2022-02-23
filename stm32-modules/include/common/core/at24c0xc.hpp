/**
 * @file at24c0xc.hpp
 * @brief Implements a generic driver for the AT24C0xC EEPROM IC's.
 */

#pragma once 

#include <array>
#include <cstring> // For memcpy

#include "core/bit_utils.hpp"

// Namespace for AT24C01C / AT24C02C driver
namespace at24c0xc {

static constexpr const size_t PAGE_LENGTH = 8;

template <typename Policy>
concept AT24C0xC_Policy = requires(Policy& policy, uint8_t addr, 
        std::array<uint8_t, PAGE_LENGTH + 1> send,
        std::array<uint8_t, PAGE_LENGTH> receive) {
    // Function to write a page (8 bytes)
    { policy.i2c_write(addr, send) } -> std::same_as<bool>;
    // Function to write a singlye byte
    { policy.i2c_write(addr, send[0]) } -> std::same_as<bool>;
    // Function to read a page (8 bytes).
    // First parameter is the device address, second
    // is the array to return data. Length of transaction
    // is deduced from the array type.
    { policy.i2c_read(addr, receive) } -> std::same_as<bool>;
    // Function to enable or disable protection.
    // True turns on protection, false disables it.
    { policy.set_write_protect(true) } -> std::same_as<void>;
};

template <size_t PAGES, uint8_t ADDRESS>
class AT24C0xC {
  public:
    // Either a 1024 or 2048 bit device
    static_assert((PAGES == 16) ||( PAGES == 32), 
                  "EEPROM size must be 1024 or 2048 bits");
    // Address is lower 7 bits
    static_assert(ADDRESS < 0x80,
                  "Address must be a 7-bit value");

    /**
     * @brief Serialize and write a value of type T to the EEPROM
     * 
     * @tparam Policy Instance of policy for sending/receiving over I2C 
     * @tparam T The type to write. Must be serializable to an 8 byte
     * or less value.
     * @param page The page number to write to.
     * @param value The value to write to \c page
     * @param policy Instance of \c T
     * @return true on success, false otherwise
     */
    template <AT24C0xC_Policy Policy, typename T>
    requires std::is_trivially_copyable_v<T>
    auto write_value(uint8_t page, T value, Policy &policy) -> bool {
        // The type to be written must be serializable to a single page
        static_assert(sizeof(T) <= PAGE_LENGTH,
                      "Type T must be 8 bytes max to serialize");
        using BufferT = std::array<uint8_t, PAGE_LENGTH + 1>;
        // Check memory bounds
        if(page > PAGES) {
            return false;
        }
        // Because T must be trivially copyable, this is not a dangerous copy
        uint64_t value_int = 0;
        std::memcpy(&value_int, &value, sizeof(T));
        // Actual address is based on the byte.
        BufferT buffer;
        buffer.at(0) = page * PAGE_LENGTH;
        auto itr = bit_utils::int_to_bytes(static_cast<uint64_t>(value_int), buffer.begin() + 1, buffer.end());
        if(itr != buffer.end()) {
            // Error converting data
            return false;
        }

        return policy.i2c_write(_address, buffer);
    }

    /**
     * @brief Read and deserialize a value of type T from EEPROM
     * 
     * @tparam Policy 
     * @tparam T 
     * @param page 
     * @param policy 
     * @return std::optional<T> 
     */
    template <AT24C0xC_Policy Policy, typename T>
    requires std::is_trivially_copyable_v<T>
    auto read_value(uint8_t page, Policy &policy) -> std::optional<T> {
        using RT = std::optional<T>;
        using BufferT = std::array<uint8_t, PAGE_LENGTH>;
        // Check memory bounds
        if(page > PAGES) {
            return RT();
        }
        // Must write the address before reading everything else
        if( !policy.i2c_write(_address, page * PAGE_LENGTH) ) {
            return RT();
        }

        BufferT buffer = {0};
        if( !policy.i2c_read(_address, buffer) ) {
            return RT();
        }
        uint64_t value_int = 0;
        auto itr = bit_utils::bytes_to_int(buffer, value_int);
        if(itr != buffer.end()) {
            return RT();
        }
        T value;
        memcpy(&value, &value_int, sizeof(T));
        return RT(value);
    }

    [[nodiscard]] auto const size() -> size_t {
        return _size;
    }

  private:  
    // Total size of the EEPROm
    static constexpr const size_t _size = PAGES * PAGE_LENGTH;
    static constexpr const uint8_t _address = ADDRESS;
    
};

}
