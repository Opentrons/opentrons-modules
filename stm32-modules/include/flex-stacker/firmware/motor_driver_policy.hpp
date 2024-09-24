#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "systemwide.h"

namespace motor_driver_policy {

static constexpr size_t MESSAGE_LEN = 5;

// The type of a single TMC2160 message.
using MessageT = std::array<uint8_t, MESSAGE_LEN>;

// Flag for whether this is a read or write
enum class WriteFlag { READ = 0x00, WRITE = 0x80 };

class MotorDriverPolicy {
  public:
    using RxTxReturn = std::optional<MessageT>;
    auto tmc2160_transmit_receive(MotorID motor_id, MessageT& data)
        -> RxTxReturn;

    auto start_stream(MotorID motor_id, MessageT& data) -> bool;

    auto stop_stream() -> bool;
};

}  // namespace motor_driver_policy
