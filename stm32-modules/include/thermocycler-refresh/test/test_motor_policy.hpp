#pragma once

#include "test/test_tmc2130_policy.hpp"

class TestMotorPolicy {
  public:

    // Functionality to fulfill concept

    auto lid_stepper_set_vref(uint16_t target_vref_mV) -> void {
        _target_vref = target_vref_mV;
    }
    auto lid_stepper_start(float angle) -> void {
        // Simulate jumping right to the end
        if(_lid_fault) { return; }
        _actual_angle = angle;
        _moving = false;
    }
    auto lid_stepper_stop() -> void {
        _moving = false;
    }

    auto lid_stepper_check_fault() -> bool {
        return _lid_fault;
    }

    auto lid_stepper_reset() -> bool {
        _moving = false;
        _target_vref = 0;
        _lid_fault = false;
        return true;
    }

    auto lid_solenoid_disengage() -> void { _solenoid_engaged = false; }  
    auto lid_solenoid_engage() -> void { _solenoid_engaged = true; }  
    auto delay_ticks(uint32_t ms) -> void { static_cast<void>(ms); return; }

    // Test-specific functions

    auto solenoid_engaged() -> bool { return _solenoid_engaged; }

    auto trigger_lid_fault() -> void { _lid_fault = true; }

  private:
    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint16_t _target_vref = 0;
    double _actual_angle = 0;
    bool _moving = false;
    bool _lid_fault = false;
};
