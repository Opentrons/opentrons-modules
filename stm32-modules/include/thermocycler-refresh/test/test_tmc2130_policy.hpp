#pragma once

#include <map>

#include "thermocycler-refresh/tmc2130.hpp"

class TestTMC2130Policy {
  public:
    using ReadRT = std::optional<uint64_t>;

    TestTMC2130Policy() : _registers() {
        _registers[(uint8_t)tmc2130::Registers::general_config] = 0;
        _registers[(uint8_t)tmc2130::Registers::general_status] = 0;
        _registers[(uint8_t)tmc2130::Registers::io_input] = 0;
        _registers[(uint8_t)tmc2130::Registers::ihold_irun] = 0;
        _registers[(uint8_t)tmc2130::Registers::tpower_down] = 0;
        _registers[(uint8_t)tmc2130::Registers::tstep] = 0;
        _registers[(uint8_t)tmc2130::Registers::tpwmthrs] = 0;
        _registers[(uint8_t)tmc2130::Registers::tcoolthrs] = 0;
        _registers[(uint8_t)tmc2130::Registers::thigh] = 0;
        _registers[(uint8_t)tmc2130::Registers::xdirect] = 0;
        _registers[(uint8_t)tmc2130::Registers::vdcmin] = 0;
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
