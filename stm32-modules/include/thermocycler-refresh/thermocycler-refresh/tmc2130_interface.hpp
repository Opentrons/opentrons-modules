/**
 * @file tmc2130_interface.hpp
 * @brief Provides a templatized SPI interface to talk to the TMC2130
 *
 */
#pragma once

#include <optional>

#include "thermocycler-refresh/tmc2130_registers.hpp"

namespace tmc2130 {

static constexpr size_t MESSAGE_LEN = 5;

// The type of a single TMC2130 message.
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

// Flag for whether this is a read or write
enum class WriteFlag { READ = 0x00, WRITE = 0x80 };

// Hardware abstraction policy for the TMC2130 communication.
template <typename Policy>
concept TMC2130InterfacePolicy = requires(Policy& p, MessageT& data) {
    // A function to read & write to a register. addr should include the
    // read/write bit.
    { p.tmc2130_transmit_receive(data) } -> std::same_as<std::optional<MessageT>>;
};

/**
 * @brief Provides SPI access to the TMC2130
 */
class TMC2130Interface {
  public:
    /**
     * @brief Build a message to send over SPI
     * @param[in] addr The address to write to
     * @param[in] mode The mode to use, either WRITE or READ
     * @param[in] val The contents to write to the address (0 if this is a read)
     * @return An array with the contents of the message, or nothing if
     * there was an error
     */
    static auto build_message(Registers addr, WriteFlag mode,
                              RegisterSerializedType val)
        -> std::optional<MessageT> {
        using RT = std::optional<MessageT>;
        MessageT buffer = {0};
        auto* iter = buffer.begin();
        auto addr_byte = static_cast<uint8_t>(addr);
        addr_byte |= static_cast<uint8_t>(mode);
        iter = bit_utils::int_to_bytes(addr_byte, iter, buffer.end());
        iter = bit_utils::int_to_bytes(val, iter, buffer.end());
        if (iter != buffer.end()) {
            return RT();
        }
        return RT(buffer);
    }

    /**
     * @brief Write to a register
     *
     * @tparam Policy Type used for bus-level SPI comms
     * @param[in] addr The address to write to
     * @param[in] val The value to write
     * @param[in] policy Instance of \c Policy
     * @return True on success, false on error to write
     */
    template <TMC2130InterfacePolicy Policy>
    auto write(Registers addr, RegisterSerializedType value, Policy& policy)
        -> bool {
        auto buffer = build_message(addr, WriteFlag::WRITE, value);
        if (!buffer.has_value()) {
            return false;
        }
        return policy.tmc2130_transmit_receive(buffer.value()).has_value();
    }

    /**
     * @brief Read from a register. This actually performs two SPI
     * transactions, as the first one will not return the correct data.
     *
     * @tparam Policy Type used for bus-level SPI comms
     * @param addr The address to read from
     * @param[in] policy Instance of \c Policy
     * @return Nothing on error, read value on success
     */
    template <TMC2130InterfacePolicy Policy>
    auto read(Registers addr, Policy& policy)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto buffer = build_message(addr, WriteFlag::READ, 0);
        if (!buffer.has_value()) {
            return RT();
        }
        auto ret = policy.tmc2130_transmit_receive(buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        ret = policy.tmc2130_transmit_receive(buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        auto* iter = ret.value().begin();
        std::advance(iter, 1);

        RegisterSerializedType retval = 0;
        iter = bit_utils::bytes_to_int(iter, ret.value().end(), retval);
        return RT(retval);
    }
};

}  // namespace tmc2130
