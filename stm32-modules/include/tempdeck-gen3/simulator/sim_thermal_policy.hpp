#pragma once

#include <cmath>
#include <cstdint>

#include "simulator/sim_at24c0xc_policy.hpp"

struct SimThermalPolicy : public at24c0xc_sim_policy::SimAT24C0XCPolicy<32> {
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

  private:
    bool _enabled = false;
    double _power = 0.0F;  // Positive for heat, negative for cool
    double _fan = 0.0F;
};
