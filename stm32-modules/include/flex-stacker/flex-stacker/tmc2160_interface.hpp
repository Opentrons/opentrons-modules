#pragma once

#include <optional>

#include "core/bit_utils.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
#include "systemwide.h"

namespace tmc2160 {

static constexpr size_t MESSAGE_LEN = 5;

// The type of a single TMC2160 message.
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

// Flag for whether this is a read or write
enum class WriteFlag { READ = 0x00, WRITE = 0x80 };

template <typename P>
concept TMC2160InterfacePolicy = requires(P p, MotorID motor_id,
                                          MessageT& message) {
    // A function to read & write to a register. addr should include the
    // read/write bit.
    {
        p.tmc2160_transmit_receive(motor_id, message)
        } -> std::same_as<std::optional<MessageT>>;
};

/**
 * @brief Provides SPI access to the TMC2160
 */
template <TMC2160InterfacePolicy Policy>
class TMC2160Interface {
  public:
    TMC2160Interface(Policy& policy) : _policy(policy) {}
    TMC2160Interface(const TMC2160Interface& c) = delete;
    TMC2160Interface(const TMC2160Interface&& c) = delete;
    auto operator=(const TMC2160Interface& c) = delete;
    auto operator=(const TMC2160Interface&& c) = delete;
    ~TMC2160Interface() = default;

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
    auto write(Registers addr, RegisterSerializedType value, MotorID motor_id)
        -> bool {
        auto buffer = build_message(addr, WriteFlag::WRITE, value);
        if (!buffer.has_value()) {
            return false;
        }
        return _policy.tmc2160_transmit_receive(motor_id, buffer.value())
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
        using RT = std::optional<RegisterSerializedType>;
        auto buffer = build_message(addr, WriteFlag::READ, 0);
        if (!buffer.has_value()) {
            return RT();
        }
        auto ret = _policy.tmc2160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        ret = _policy.tmc2160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        auto* iter = ret.value().begin();
        std::advance(iter, 1);

        RegisterSerializedType retval = 0;
        iter = bit_utils::bytes_to_int(iter, ret.value().end(), retval);
        return RT(retval);
    }

    auto create_stallguard_message() -> MessageT {
        return build_message(Registers::DRVSTATUS, WriteFlag::READ, 0).value();
    }

    auto stream_stallguard(MotorID motor_id)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto buffer = create_stallguard_message();
        auto ret = _policy.tmc2160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return RT();
        }


        for (;;) {

            ret = _policy.tmc2160_transmit_receive(motor_id, buffer.value());
            if (!ret.has_value()) {
                return RT();
            }
            ret = _policy.tmc2160_transmit_receive(motor_id, buffer.value());
            if (!ret.has_value()) {
                return RT();
            }
            auto* iter = ret.value().begin();
            std::advance(iter, 1);

            RegisterSerializedType retval = 0;
            iter = bit_utils::bytes_to_int(iter, ret.value().end(), retval);
        }
    }

  private:
    Policy& _policy;
};

}  // namespace tmc2160
