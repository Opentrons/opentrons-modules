#include "firmware/thermal_policy.hpp"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::enable_peltier() -> void { return; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::disable_peltier() -> void { return; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_peltier_heat_power(double power) -> bool {
    static_cast<void>(power);
    return true;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_peltier_cool_power(double power) -> bool {
    static_cast<void>(power);
    return true;
}
