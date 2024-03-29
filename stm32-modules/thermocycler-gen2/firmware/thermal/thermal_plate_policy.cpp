#include "firmware/thermal_plate_policy.hpp"

#include "FreeRTOS.h"
#include "firmware/thermal_fan_hardware.h"
#include "firmware/thermal_peltier_hardware.h"
#include "systemwide.h"
#include "task.h"

using namespace plate_policy;

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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::get_fan_rpm() -> std::pair<double, double> {
    auto t1 = thermal_fan_get_tach_1_rpm();
    auto t2 = thermal_fan_get_tach_2_rpm();
    return std::make_pair(t1, t2);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::set_write_protect(bool write_protect) -> void {
    thermal_eeprom_set_write_protect(write_protect);
    if (!write_protect) {
        // When done writing to the EEPROM, it needs some time to perform
        // the write. We could poll the I2C bus, or we can just add a short
        // delay since this won't happen during any critical sections.
        static constexpr const TickType_t delay_ticks = pdMS_TO_TICKS(10);
        vTaskDelay(delay_ticks);
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPlatePolicy::i2c_write(uint8_t addr, uint8_t data) -> bool {
    return thermal_i2c_write_data(addr, &data, 1);
}
