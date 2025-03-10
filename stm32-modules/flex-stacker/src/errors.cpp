#include "flex-stacker/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement";
const char* const ESTOP_TRIGGERED = "ERR006:estop triggered";
const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout";
const char* const SYSTEM_EEPROM_ERROR =
    "ERR303:system:EEPROM communication error";
const char* const SYSTEM_SET_STATUSBAR_COLOR_ERROR =
    "ERR304:system:STATUSBAR communication error";

const char* const TMF8820_COMM_ERROR = "ERR801:TMF8820 Comm error";
const char* const TMF8820_MEASURE_ERROR = "ERR802:TMF8820 Measure error";

const char* const TMC2160_READ_ERROR = "ERR901:TMC2160 driver read error";
const char* const TMC2160_WRITE_ERROR = "ERR902:TMC2160 driver write error";
const char* const TMC2160_INVALID_ADDRESS = "ERR903:TMC2160 invalid address";
const char* const TMC2160_INVALID_VALUE = "ERR904:TMC2160 invalid value";

const char* const MOTOR_ENABLE_FAILED = "ERR401:motor enable error";
const char* const MOTOR_DISABLE_FAILED = "ERR402:motor disable error";
const char* const MOTOR_STALL_DETECTED = "ERR403:motor stall error";
const char* const MOTOR_QUEUE_FULL = "ERR404:motor queue full error";
const char* const UNEXPECTED_LIMIT_SWITCH =
    "ERR405:limit switch triggered unexpectedly";

const char* const X_MOTOR_BUSY = "ERR501:X motor busy error";
const char* const Z_MOTOR_BUSY = "ERR502:Z motor busy error";
const char* const L_MOTOR_BUSY = "ERR503:L motor busy error";
const char* const STOP_REQUESTED = "ERR504:stop requested";

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
        HANDLE_CASE(ESTOP_TRIGGERED);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
        HANDLE_CASE(SYSTEM_EEPROM_ERROR);
        HANDLE_CASE(SYSTEM_SET_STATUSBAR_COLOR_ERROR);
        HANDLE_CASE(TMF8820_COMM_ERROR);
        HANDLE_CASE(TMF8820_MEASURE_ERROR);
        HANDLE_CASE(TMC2160_READ_ERROR);
        HANDLE_CASE(TMC2160_WRITE_ERROR);
        HANDLE_CASE(TMC2160_INVALID_ADDRESS);
        HANDLE_CASE(TMC2160_INVALID_VALUE);
        HANDLE_CASE(MOTOR_ENABLE_FAILED);
        HANDLE_CASE(MOTOR_DISABLE_FAILED);
        HANDLE_CASE(MOTOR_STALL_DETECTED);
        HANDLE_CASE(MOTOR_QUEUE_FULL);
        HANDLE_CASE(UNEXPECTED_LIMIT_SWITCH);
        HANDLE_CASE(X_MOTOR_BUSY);
        HANDLE_CASE(Z_MOTOR_BUSY);
        HANDLE_CASE(L_MOTOR_BUSY);
        HANDLE_CASE(STOP_REQUESTED);
    }
    return UNKNOWN_ERROR;
}
