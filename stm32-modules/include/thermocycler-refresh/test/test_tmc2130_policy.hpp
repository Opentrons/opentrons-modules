#pragma once

#include <map>

#include "thermocycler-refresh/tmc2130.hpp"

class TestTMC2130Policy {
  public:
    using ReadRT = std::optional<uint64_t>;

    TestTMC2130Policy() : _registers() {
        _registers[(uint8_t)tmc2130::Registers::GCONF] = 0;
        _registers[(uint8_t)tmc2130::Registers::GSTAT] = 0;
        _registers[(uint8_t)tmc2130::Registers::IOIN] = 0;
        _registers[(uint8_t)tmc2130::Registers::IHOLD_IRUN] = 0;
        _registers[(uint8_t)tmc2130::Registers::TPOWERDOWN] = 0;
        _registers[(uint8_t)tmc2130::Registers::TSTEP] = 0;
        _registers[(uint8_t)tmc2130::Registers::TPWMTHRS] = 0;
        _registers[(uint8_t)tmc2130::Registers::TCOOLTHRS] = 0;
        _registers[(uint8_t)tmc2130::Registers::THIGH] = 0;
        _registers[(uint8_t)tmc2130::Registers::XDIRECT] = 0;
        _registers[(uint8_t)tmc2130::Registers::VDCMIN] = 0;
        _registers[(uint8_t)tmc2130::Registers::CHOPCONF] = 0;
        _registers[(uint8_t)tmc2130::Registers::COOLCONF] = 0;
        _registers[(uint8_t)tmc2130::Registers::DCCTRL] = 0;
        _registers[(uint8_t)tmc2130::Registers::DRVSTATUS] = 0;
        _registers[(uint8_t)tmc2130::Registers::PWMCONF] = 0;
        _registers[(uint8_t)tmc2130::Registers::ENCM_CTRL] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_0] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_1] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_2] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_3] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_4] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_5] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_6] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUT_7] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUTSEL] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSLUTSTART] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSCNT] = 0;
        _registers[(uint8_t)tmc2130::Registers::MSCURACT] = 0;
        _registers[(uint8_t)tmc2130::Registers::PWM_SCALE] = 0;
        _registers[(uint8_t)tmc2130::Registers::LOST_STEPS] = 0;
    }

    auto write_register(tmc2130::Registers addr, uint64_t value) -> bool {
        if (_registers.count((uint8_t)addr) == 0) {
            return false;
        }
        _registers[(uint8_t)addr] = value;
        return true;
    }

    auto read_register(tmc2130::Registers addr) -> ReadRT {
        if (_registers.count((uint8_t)addr) == 0) {
            return ReadRT();
        }
        return ReadRT(_registers[(uint8_t)addr]);
    }

  private:
    using RegMap = std::map<uint8_t, uint64_t>;
    RegMap _registers;
};
