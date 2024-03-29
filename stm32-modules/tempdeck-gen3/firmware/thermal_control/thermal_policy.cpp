#include "firmware/thermal_policy.hpp"

#include "firmware/i2c_hardware.h"
#include "firmware/tachometer_hardware.h"
#include "firmware/thermal_hardware.h"

namespace thermal_policy {
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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_fan_power(double power) -> bool {
    return thermal_hardware_set_fan_power(power);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto ThermalPolicy::get_fan_rpm() const -> double {
    return tachometer_hardware_get_rpm();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::set_write_protect(bool set) -> void {
    thermal_hardware_set_eeprom_write_protect(set);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto ThermalPolicy::i2c_write(uint8_t addr, uint8_t data) -> bool {
    return i2c_hardware_write_data(I2C_BUS_THERMAL, addr, &data, 1);
}

}  // namespace thermal_policy
