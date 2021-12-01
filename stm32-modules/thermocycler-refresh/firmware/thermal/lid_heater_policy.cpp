#include "firmware/lid_heater_policy.hpp"

#include <algorithm>

#include "firmware/thermal_heater_hardware.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto LidHeaterPolicy::set_heater_power(double power) -> bool {
    // Set the power of the heater between 0 and 1
    power = std::clamp(power, (double)0.0F, (double)1.0F);
    return thermal_heater_set_power(power);
}
