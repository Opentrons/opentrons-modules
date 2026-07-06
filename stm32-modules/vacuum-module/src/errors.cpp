#include "vacuum-module/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement";
const char* const TASK_NOT_READY = "ERR007:task not ready";
const char* const DEBUG_MESSAGE = "DEBUG:";

const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout";
const char* const SYSTEM_EEPROM_ERROR =
    "ERR303:system:EEPROM communication error";

// 4xx - Vacuum Errors
const char* const PRESSURE_NOT_REACHED_ERROR =
    "ERR400:vacuum:target pressure not reached";
const char* const WASTE_FULL_ERROR = "ERR401:vacuum:waste is full";
const char* const VENT_FAILED_ERROR = "ERR402:vacuum:solenoid failed to vent";
// 5xx - Pump Errors

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
        HANDLE_CASE(TASK_NOT_READY);
        HANDLE_CASE(DEBUG_MESSAGE);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
        HANDLE_CASE(SYSTEM_EEPROM_ERROR);
        HANDLE_CASE(PRESSURE_NOT_REACHED_ERROR);
        HANDLE_CASE(WASTE_FULL_ERROR);
        HANDLE_CASE(VENT_FAILED_ERROR);
    }
    return UNKNOWN_ERROR;
}
