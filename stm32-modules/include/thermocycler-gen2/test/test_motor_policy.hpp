#pragma once

#include <functional>

#include "test/test_tmc2130_policy.hpp"

class TestMotorPolicy : public TestTMC2130Policy {
  public:
    using Callback = std::function<void()>;

    TestMotorPolicy() : TestTMC2130Policy(), _callback() {}

    // Functionality to fulfill concept

    static constexpr uint32_t MotorTickFrequency = 1000000;

    auto lid_stepper_set_dac(uint8_t dac_val) -> void { _dac_val = dac_val; }
    auto lid_stepper_start(int32_t steps, bool overdrive) -> void {
        std::cout << "lid stepper start" << std::endl;
        std::cout << "steps = " << steps << std::endl;
        _lid_overdrive = overdrive;
        // Simulate jumping right to the end
        if (_lid_fault) {
            std::cout << "LID FAULt AHHHHHHHHHHHHH\n" << std::endl;
            return;
        }
        _actual_angle += steps;
        _lid_moving = false;
        // angle gets changed here
    }
    auto lid_stepper_stop() -> void { _lid_moving = false; }

    auto lid_stepper_check_fault() -> bool { return _lid_fault; }

    auto lid_stepper_reset() -> bool {
        _lid_moving = false;
        _dac_val = 0;
        _lid_fault = false;
        return true;
    }

    auto lid_stepper_set_rpm(double rpm) -> bool {
        _lid_rpm = rpm;
        return true;
    }

    auto lid_solenoid_disengage() -> void { _solenoid_engaged = false; }
    auto lid_solenoid_engage() -> void { _solenoid_engaged = true; }

    auto lid_read_closed_switch() -> bool { return _lid_closed_switch; }

    auto lid_read_open_switch() -> bool { return _lid_open_switch; }

    auto seal_stepper_start(Callback cb) -> bool {
        if (_seal_moving) {
            return false;
        }

        _seal_moving = true;
        _callback = cb;
        return true;
    }

    auto seal_stepper_stop() -> void { _seal_moving = false; }

    auto seal_switch_set_extension_armed() -> void {
        _extension_switch_armed = true;
    }

    auto seal_switch_set_retraction_armed() -> void {
        _retraction_switch_armed = true;
    }

    auto seal_switch_set_disarmed() -> void {
        _extension_switch_armed = false;
        _retraction_switch_armed = false;
    }

    auto seal_read_extension_switch() -> bool {
        return _extension_switch_triggered;
    }

    auto seal_read_retraction_switch() -> bool {
        return _retraction_switch_triggered;
    }

    auto seal_switches_are_shared() -> bool { return _shared_switch_lines; }

    // Test-specific functions

    auto tick() -> void {
//        std::cout << "tick called" << std::endl;
        if (_seal_moving) {
            _callback();
        }
    }
    auto solenoid_engaged() -> bool { return _solenoid_engaged; }
    /** Next movement will be an error.*/
    auto trigger_lid_fault() -> void { _lid_fault = true; }
    auto get_vref() -> uint8_t { return _dac_val; }
    auto get_angle() -> int32_t { return _actual_angle; }
    auto seal_moving() -> bool { return _seal_moving; }

    auto set_lid_open_switch(bool val) -> void { _lid_open_switch = val; }

    auto set_lid_closed_switch(bool val) -> void { _lid_closed_switch = val; }

    auto get_lid_overdrive() -> bool { return _lid_overdrive; }

    auto extension_switch_is_armed() -> bool { return _extension_switch_armed; }

    auto retraction_switch_is_armed() -> bool {
        return _retraction_switch_armed;
    }

    auto set_extension_switch_triggered(bool val) -> void {
        _extension_switch_triggered = val;
    }

    auto set_retraction_switch_triggered(bool val) -> void {
        _retraction_switch_triggered = val;
    }

    auto get_lid_rpm() -> double { return _lid_rpm; }

    auto set_switch_lines_shared(bool shared) -> void {
        _shared_switch_lines = shared;
    }

  private:
    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint8_t _dac_val = 0;
    int32_t _actual_angle = 0;
    bool _lid_moving = false;
    bool _lid_fault = false;
    bool _seal_moving = false;
    bool _lid_open_switch = false;
    bool _lid_closed_switch = false;
    bool _lid_overdrive = false;
    bool _extension_switch_triggered = false;
    bool _retraction_switch_triggered = false;
    bool _extension_switch_armed = false;
    bool _retraction_switch_armed = false;
    double _lid_rpm = 0;
    // Default to shared switch lines (pre-DVT)
    bool _shared_switch_lines = true;
    Callback _callback;
};
