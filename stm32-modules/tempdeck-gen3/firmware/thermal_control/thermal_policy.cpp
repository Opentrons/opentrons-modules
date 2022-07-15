#include "firmware/thermal_policy.hpp"

#include "firmware/thermal_hardware.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::enable_peltier() -> void {
    thermal_hardware_enable_peltiers();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::disable_peltier() -> void {
    thermal_hardware_disable_peltiers();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_peltier_heat_power(double power) -> bool {
    return thermal_hardware_set_peltier_heat(power);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_peltier_cool_power(double power) -> bool {
    return thermal_hardware_set_peltier_cool(power);
}
