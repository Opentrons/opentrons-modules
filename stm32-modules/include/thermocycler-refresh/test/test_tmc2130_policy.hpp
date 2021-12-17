#pragma once

#include <iostream>
#include <map>

#include "core/bit_utils.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

class TestTMC2130Policy {
  public:
    using ReadRT = std::optional<tmc2130::RegisterSerializedType>;

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

    auto transmit_receive(tmc2130::MessageT& data)
        -> std::optional<tmc2130::MessageT> {
        using RT = std::optional<tmc2130::MessageT>;
        auto iter = data.begin();
        uint8_t addr;
        tmc2130::RegisterSerializedType value;
        iter = bit_utils::bytes_to_int(iter, data.end(), addr);
        iter = bit_utils::bytes_to_int(iter, data.end(), value);

        auto mode = addr & 0x80;
        addr &= ~0x80;

        // Check this is a valid register
        if (_registers.count(addr) == 0) {
            return RT();
        }
        if (mode == static_cast<uint8_t>(tmc2130::WriteFlag::WRITE)) {
            // This is a write, so overwrite the register value
            _registers[addr] = value;
        }
        tmc2130::MessageT ret;
        iter = ret.begin();
        iter = bit_utils::int_to_bytes(get_status(), iter, ret.end());
        iter = bit_utils::int_to_bytes(_cache, iter, ret.end());
        _cache = _registers[addr];
        return RT(ret);
    }

    /** Primarily for test integration.*/
    auto read_register(tmc2130::Registers reg) -> ReadRT {
        auto addr = static_cast<uint8_t>(reg);
        if (_registers.count(addr) == 0) {
            return ReadRT(0);
        }
        return ReadRT(_registers[addr]);
    }

  private:
    auto get_status() -> uint8_t { return 0x00; }

    using RegMap = std::map<uint8_t, tmc2130::RegisterSerializedType>;
    RegMap _registers;
    tmc2130::RegisterSerializedType _cache = 0;
};
