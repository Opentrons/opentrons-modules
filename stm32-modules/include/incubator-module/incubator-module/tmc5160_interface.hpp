#pragma once

#include <optional>

#include "core/bit_utils.hpp"
#include "incubator-module/tmc5160_registers.hpp"
#include "systemwide.h"

namespace tmc5160 {

static constexpr size_t MESSAGE_LEN = 5;

// The type of a single TMC5160 message.
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

// Flag for whether this is a read or write
enum class WriteFlag : uint8_t { READ = 0x00, WRITE = 0x80 };

template <typename P>
concept TMC5160InterfacePolicy = requires(P p, MotorID motor_id,
                                          MessageT& message) {
    // A function to read & write to a register. addr should include the
    // read/write bit.
    {
        p.tmc5160_transmit_receive(motor_id, message)
        } -> std::same_as<std::optional<MessageT>>;
};

/**
 * @brief Provides SPI access to the TMC5160
 */
template <TMC5160InterfacePolicy Policy>
class TMC5160Interface {
  public:
    TMC5160Interface(Policy& policy) : _policy(policy) {}
    TMC5160Interface(const TMC5160Interface& c) = delete;
    TMC5160Interface(const TMC5160Interface&& c) = delete;
    auto operator=(const TMC5160Interface& c) = delete;
    auto operator=(const TMC5160Interface&& c) = delete;
    ~TMC5160Interface() = default;

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
        MessageT buffer = {0};
        auto* iter = buffer.begin();
        auto addr_byte = static_cast<uint8_t>(addr);
        addr_byte |= static_cast<uint8_t>(mode);
        iter = bit_utils::int_to_bytes(addr_byte, iter, buffer.end());
        iter = bit_utils::int_to_bytes(val, iter, buffer.end());
        if (iter != buffer.end()) {
            return {};
        }
        return {buffer};
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
    auto write(Registers addr, RegisterSerializedType value, MotorID motor_id)
        -> bool {
        auto buffer = build_message(addr, WriteFlag::WRITE, value);
        if (!buffer.has_value()) {
            return false;
        }
        return _policy.tmc5160_transmit_receive(motor_id, buffer.value())
            .has_value();
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
    auto read(Registers addr, MotorID motor_id)
        -> std::optional<RegisterSerializedType> {
        auto buffer = build_message(addr, WriteFlag::READ, 0);
        if (!buffer.has_value()) {
            return {};
        }
        auto ret = _policy.tmc5160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return {};
        }
        ret = _policy.tmc5160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return {};
        }
        auto* iter = ret.value().begin();
        std::advance(iter, 1);

        RegisterSerializedType retval = 0;
        iter = bit_utils::bytes_to_int(iter, ret.value().end(), retval);
        return {retval};
    }

  private:
    Policy& _policy;
};

}  // namespace tmc5160
