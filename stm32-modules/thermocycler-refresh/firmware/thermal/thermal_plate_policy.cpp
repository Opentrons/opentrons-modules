#include "firmware/thermal_plate_policy.hpp"

#include "firmware/thermal_peltier_hardware.h"
#include "systemwide.h"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_enabled(bool enabled) -> void {
    thermal_peltier_set_enable(enabled);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_peltier(PeltierID peltier, double power,
                                     PeltierDirection direction) -> bool {
    if (!thermal_peltier_get_enable()) {
        return false;
    }
    if (peltier == PELTIER_NUMBER) {
        return false;
    }

    thermal_peltier_set_power(peltier, power, direction);
    return true;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::get_peltier(PeltierID peltier)
    -> std::pair<PeltierDirection, double> {
    using RT = std::pair<PeltierDirection, double>;
    if (!thermal_peltier_get_enable()) {
        return RT(PeltierDirection::PELTIER_HEATING, 0.0F);
    }
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
