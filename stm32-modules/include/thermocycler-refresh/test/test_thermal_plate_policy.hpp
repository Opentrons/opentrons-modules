#pragma once

#include "test/test_at24c0xc_policy.hpp"
#include "thermocycler-refresh/thermal_general.hpp"

struct TestPeltier {
    TestPeltier() { reset(); }
    float power = 0.0F;
    PeltierDirection direction = PeltierDirection::PELTIER_HEATING;

    auto reset() -> void {
        power = 0.0F;
        direction = PeltierDirection::PELTIER_HEATING;
    }
};

class TestThermalPlatePolicy
    : public at24c0xc_test_policy::TestAT24C0XCPolicy<32> {
  public:
    TestThermalPlatePolicy() : at24c0xc_test_policy::TestAT24C0XCPolicy<32>() {}

    auto set_enabled(bool enabled) -> void {
        _enabled = enabled;
        if (!enabled) {
            _left.reset();
            _center.reset();
            _right.reset();
        }
    }

    auto set_peltier(PeltierID peltier, double power,
                     PeltierDirection direction) -> bool {
        auto handle = get_peltier_from_id(peltier);
        if (!handle.has_value()) {
            return false;
        }
        if (power == 0.0F) {
            direction = PELTIER_HEATING;
        }
        handle.value().get().direction = direction;
        handle.value().get().power = power;

        return true;
    }

    auto get_peltier(PeltierID peltier) -> std::pair<PeltierDirection, double> {
        using RT = std::pair<PeltierDirection, double>;
        auto handle = get_peltier_from_id(peltier);
        if (!handle.has_value()) {
            return RT(PeltierDirection::PELTIER_COOLING, 0.0F);
        }
        return RT(handle.value().get().direction, handle.value().get().power);
    }

    auto set_fan(double power) -> bool {
        power = std::clamp(power, (double)0.0F, (double)1.0F);
        _fan_power = power;
        return true;
    }

    auto get_fan() -> double { return _fan_power; }

    bool _enabled = false;
    TestPeltier _left = TestPeltier();
    TestPeltier _center = TestPeltier();
    TestPeltier _right = TestPeltier();
    double _fan_power = 0.0F;

  private:
    using GetPeltierT = std::optional<std::reference_wrapper<TestPeltier>>;
    auto get_peltier_from_id(PeltierID peltier) -> GetPeltierT {
        if (peltier == PeltierID::PELTIER_LEFT) {
            return GetPeltierT(_left);
        }
        if (peltier == PeltierID::PELTIER_RIGHT) {
            return GetPeltierT(_right);
        }
        if (peltier == PeltierID::PELTIER_CENTER) {
            return GetPeltierT(_center);
        }
        return GetPeltierT();
    }
};
