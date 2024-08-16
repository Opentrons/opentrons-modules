#include "flex-stacker/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun\n";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full\n";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode\n";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full\n";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement\n";
const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format\n";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout\n";
const char* const SYSTEM_EEPROM_ERROR =
    "ERR303:system:EEPROM communication error\n";
const char* const TMC2160_READ_ERROR = "ERR901:TMC2160 driver read error\n";
const char* const TMC2160_WRITE_ERROR = "ERR902:TMC2160 driver write error\n";
const char* const TMC2160_INVALID_ADDRESS = "ERR903:TMC2160 invalid address\n";

const char* const MOTOR_ENABLE_FAILED = "ERR401:motor enable error\n";
const char* const MOTOR_DISABLE_FAILED = "ERR401:motor disable error\n";


const char* const UNKNOWN_ERROR = "ERR-1:unknown error code\n";

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define HANDLE_CASE(errname) \
    case ErrorCode::errname: \
        return errname

auto errors::errorstring(ErrorCode code) -> const char* {
    switch (code) {
        HANDLE_CASE(NO_ERROR);
        HANDLE_CASE(USB_TX_OVERRUN);
        HANDLE_CASE(INTERNAL_QUEUE_FULL);
        HANDLE_CASE(UNHANDLED_GCODE);
        HANDLE_CASE(GCODE_CACHE_FULL);
        HANDLE_CASE(BAD_MESSAGE_ACKNOWLEDGEMENT);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
        HANDLE_CASE(SYSTEM_EEPROM_ERROR);
        HANDLE_CASE(TMC2160_READ_ERROR);
        HANDLE_CASE(TMC2160_WRITE_ERROR);
        HANDLE_CASE(TMC2160_INVALID_ADDRESS);
        HANDLE_CASE(MOTOR_ENABLE_FAILED);
        HANDLE_CASE(MOTOR_DISABLE_FAILED);

    }
    return UNKNOWN_ERROR;
}
