#pragma once
#include <array>
#include <variant>

namespace messages {
struct SetRPMMessage {
    uint32_t rpm;
};
struct DeactivateMessage {};
struct HomeMessage {};
struct SetTempMessage {
    uint32_t target_temperature;
};
struct UpdateTemperatureMessage {
    float temperature;
};

struct UpdateRPMMessage {
    uint32_t rpm;
};

constexpr size_t RESPONSE_LENGTH = 128;

struct SendResponseMessage {
    std::array<char, RESPONSE_LENGTH> msg;
};

using HeaterMessage = ::std::variant<SetTempMessage, DeactivateMessage>;
using MotorMessage =
    ::std::variant<SetRPMMessage, DeactivateMessage, HomeMessage>;
using UIMessage = ::std::variant<UpdateTemperatureMessage, UpdateRPMMessage>;
using HostCommsMessage = ::std::variant<SendResponseMessage>;
};  // namespace messages
