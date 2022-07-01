#pragma once

#include "systemwide.h"
#include "tempdeck-gen3/errors.hpp"

struct TestSystemPolicy {
    using Serial = std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>;
    void enter_bootloader(void) { ++_bootloader_count; }

    errors::ErrorCode set_serial_number(Serial ser) {
        _serial = ser;
        _serial_set = true;
        return errors::ErrorCode::NO_ERROR;
    }

    Serial get_serial_number() {
        if (_serial_set) {
            return _serial;
        }
        Serial empty_serial = {"EMPTYSN"};
        return empty_serial;
    }

    int _bootloader_count = 0;
    Serial _serial = {'x'};
    bool _serial_set = false;
};
