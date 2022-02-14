#include "firmware/thermal_plate_policy.hpp"

#include "firmware/thermal_fan_hardware.h"
#include "firmware/thermal_peltier_hardware.h"
#include "systemwide.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_enabled(bool enabled) -> void {
    thermal_peltier_set_enable(enabled);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_peltier(PeltierID peltier, double power,
                                     PeltierDirection direction) -> bool {
    if (peltier == PELTIER_NUMBER) {
        return false;
    }

    return thermal_peltier_set_power(peltier, power, direction);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::get_peltier(PeltierID peltier)
    -> std::pair<PeltierDirection, double> {
    using RT = std::pair<PeltierDirection, double>;
    if (peltier == PELTIER_NUMBER) {
        return RT(PeltierDirection::PELTIER_HEATING, 0.0F);
    }

    PeltierDirection dir = PeltierDirection::PELTIER_COOLING;
    double pwr = 0.0F;
    if (thermal_peltier_get_power(peltier, &pwr, &dir)) {
        return RT(dir, pwr);
    }

    return RT(PeltierDirection::PELTIER_HEATING, 0.0F);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_fan(double power) -> bool {
    power = std::clamp(power, (double)0.0F, (double)1.0F);
    return thermal_fan_set_power(power);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::get_fan() -> double { return thermal_fan_get_power(); }
