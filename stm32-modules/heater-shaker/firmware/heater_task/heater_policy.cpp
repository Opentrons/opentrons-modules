#include "heater_policy.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "heater_hardware.h"
#pragma GCC diagnostic pop

HeaterPolicy::HeaterPolicy(heater_hardware* hardware)
    : hardware_handle(hardware) {}

// These functions only return literals to keep the commits clean
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto HeaterPolicy::power_good() const -> bool { return true; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto HeaterPolicy::try_reset_power_good() -> bool { return true; }
