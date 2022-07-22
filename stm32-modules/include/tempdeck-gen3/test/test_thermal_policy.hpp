#pragma once

#include <cmath>
#include <cstdint>

struct TestThermalPolicy {
    auto enable_peltier() -> void { _enabled = true; }

    auto disable_peltier() -> void { _enabled = false; }

    auto set_peltier_heat_power(double power) -> bool {
        if (!_enabled) {
            return false;
        }
        _power = std::min(1.0, std::abs(power));
        return true;
    }

    auto set_peltier_cool_power(double power) -> bool {
        if (!_enabled) {
            return false;
        }
        _power = -std::min(1.0, std::abs(power));
        return true;
    }

    auto set_fan_power(double power) -> bool {
        _fans = std::clamp(power, (double)0.0F, (double)1.0F);
        return true;
    }

    // Test integration functions

    auto is_cooling() -> bool { return _enabled && (_power < 0.0); }

    auto is_heating() -> bool { return _enabled && (_power > 0.0); }

    bool _enabled = false;
    double _power = 0.0F;  // Positive for heat, negative for cool
    double _fans = 0.0F;
};
