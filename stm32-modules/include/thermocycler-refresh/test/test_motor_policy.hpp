#pragma once

#include "test/test_tmc2130_policy.hpp"

class TestMotorPolicy {
  public:
    // Functionality to fulfill concept

    auto lid_stepper_set_dac(uint8_t dac_val) -> void { _dac_val = dac_val; }
    auto lid_stepper_start(int32_t steps) -> void {
        // Simulate jumping right to the end
        if (_lid_fault) {
            return;
        }
        _actual_angle += steps;
        _moving = false;
    }
    auto lid_stepper_stop() -> void { _moving = false; }

    auto lid_stepper_check_fault() -> bool { return _lid_fault; }

    auto lid_stepper_reset() -> bool {
        _moving = false;
        _dac_val = 0;
        _lid_fault = false;
        return true;
    }

    auto lid_solenoid_disengage() -> void { _solenoid_engaged = false; }
    auto lid_solenoid_engage() -> void { _solenoid_engaged = true; }

    // Test-specific functions

    auto solenoid_engaged() -> bool { return _solenoid_engaged; }

    /** Next movement will be an error.*/
    auto trigger_lid_fault() -> void { _lid_fault = true; }

    auto get_vref() -> uint8_t { return _dac_val; }

    auto get_angle() -> double { return _actual_angle; }

  private:
    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint8_t _dac_val = 0;
    int32_t _actual_angle = 0;
    bool _moving = false;
    bool _lid_fault = false;
};
