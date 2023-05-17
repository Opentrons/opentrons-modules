#pragma once

#include <cmath>
#include <cstdint>

#include "test/test_m24128_policy.hpp"

struct SimThermalPolicy : public m24128_test_policy::TestM24128Policy {
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
        _fan = std::clamp(power, double(0.0), double(1.0));
        return true;
    }

    auto get_fan_rpm() -> double {
        // From the fan datasheet
        static constexpr double MAX_RPM = 10800;
        return _fan * MAX_RPM;
    }

  private:
    bool _enabled = false;
    double _power = 0.0F;  // Positive for heat, negative for cool
    double _fan = 0.0F;
};
