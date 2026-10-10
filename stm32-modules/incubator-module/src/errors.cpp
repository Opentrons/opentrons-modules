#include "incubator-module/errors.hpp"

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
const char* const MOTOR_ENABLE_FAILED = "ERR401:motor enable failed";
const char* const MOTOR_DISABLE_FAILED = "ERR402:motor disable failed";
const char* const MOTOR_QUEUE_FULL = "ERR404:motor queue full";
const char* const R_MOTOR_BUSY = "ERR501:r motor busy";
const char* const STOP_REQUESTED = "ERR504:stop requested";
const char* const CAPACITIVE_SENSOR_ERROR =
    "ERR601:capacitive sensor reading failed";
const char* const TMC5160_READ_ERROR = "ERR901:tmc5160 read error";
const char* const TMC5160_WRITE_ERROR = "ERR902:tmc5160 write error";
const char* const TMC5160_INVALID_ADDRESS = "ERR903:tmc5160 invalid address";
const char* const TMC5160_INVALID_VALUE = "ERR904:tmc5160 invalid value";

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
        HANDLE_CASE(MOTOR_ENABLE_FAILED);
        HANDLE_CASE(MOTOR_DISABLE_FAILED);
        HANDLE_CASE(MOTOR_QUEUE_FULL);
        HANDLE_CASE(R_MOTOR_BUSY);
        HANDLE_CASE(STOP_REQUESTED);
        HANDLE_CASE(CAPACITIVE_SENSOR_ERROR);
        HANDLE_CASE(TMC5160_READ_ERROR);
        HANDLE_CASE(TMC5160_WRITE_ERROR);
        HANDLE_CASE(TMC5160_INVALID_ADDRESS);
        HANDLE_CASE(TMC5160_INVALID_VALUE);
    }
    return UNKNOWN_ERROR;
}
