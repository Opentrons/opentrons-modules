#include "heater-shaker/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun\n";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full\n";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode\n";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full\n";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement\n";
const char* const MOTOR_FOC_DURATION = "ERR101:main motor:FOC_DURATION\n";
const char* const MOTOR_BLDC_OVERVOLT = "ERR102:main motor:overvolt\n";
const char* const MOTOR_BLDC_UNDERVOLT = "ERR103:main motor:undervolt\n";
const char* const MOTOR_BLDC_OVERTEMP = "ERR104:main motor:overtemp\n";
const char* const MOTOR_BLDC_STARTUP_FAILED =
    "ERR105:main motor:startup failed\n";
const char* const MOTOR_BLDC_SPEEDSENSOR_FAILED =
    "ERR106:main motor:speedsensor failed\n";
const char* const MOTOR_BLDC_OVERCURRENT = "ERR107:main motor:overcurrent\n";
const char* const MOTOR_BLDC_DRIVER_ERROR = "ERR108:main motor:driver error\n";
const char* const MOTOR_SPURIOUS_ERROR = "ERR109:main motor:spurious error\n";
const char* const MOTOR_UNKNOWN_ERROR = "ERR110:main motor:unknown error\n";
const char* const MOTOR_UNABLE_TO_MOVE = "ERR111:main motor:unable to move\n";
const char* const MOTOR_ILLEGAL_SPEED = "ERR120:main motor:illegal speed\n";
const char* const MOTOR_ILLEGAL_RAMP_RATE =
    "ERR121:main motor:illegal ramp rate\n";
const char* const MOTOR_BAD_HOME = "ERR122:main motor:bad home\n";
const char* const MOTOR_NOT_HOME = "ERR123:main motor:not home (required)\n";
const char* const MOTOR_NOT_STOPPED =
    "ERR124:main motor:not stopped (required)\n";
const char* const PLATE_LOCK_TIMEOUT = "ERR125:plate lock:timeout\n";
const char* const PLATE_LOCK_NOT_CLOSED =
    "ERR126:main motor:plate lock not closed (required)\n";
const char* const HEATER_THERMISTOR_A_DISCONNECTED =
    "ERR201:heater:thermistor a disconnected\n";
const char* const HEATER_THERMISTOR_A_SHORT =
    "ERR202:heater:thermistor a short\n";
const char* const HEATER_THERMISTOR_A_OVERTEMP =
    "ERR203:heater:thermistor a overtemp\n";
const char* const HEATER_THERMISTOR_B_DISCONNECTED =
    "ERR205:heater:thermistor b disconnected\n";
const char* const HEATER_THERMISTOR_B_SHORT =
    "ERR206:heater:thermistor b short\n";
const char* const HEATER_THERMISTOR_B_OVERTEMP =
    "ERR207:heater:thermistor b overtemp\n";
const char* const HEATER_THERMISTOR_BOARD_SHORT =
    "ERR208:heater:board thermistor short\n";
const char* const HEATER_THERMISTOR_BOARD_OVERTEMP =
    "ERR209:heater:board thermistor overtemp\n";
const char* const HEATER_THERMISTOR_BOARD_DISCONNECTED =
    "ERR210:heater:board thermistor disconnected\n";
const char* const HEATER_HARDWARE_ERROR_LATCH =
    "ERR211:heater:hardware error latch set\n";
const char* const HEATER_CONSTANT_OUT_OF_RANGE =
    "ERR212:heater:control constant out of range\n";
const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format\n";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout\n";
const char* const SYSTEM_LED_I2C_NOT_READY =
    "ERR303:system:LED I2C not ready\n";
const char* const SYSTEM_LED_TRANSMIT_ERROR =
    "ERR304:system:LED I2C transmission or FreeRTOS notification passing "
    "failed\n";

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
        HANDLE_CASE(MOTOR_FOC_DURATION);
        HANDLE_CASE(MOTOR_BLDC_OVERVOLT);
        HANDLE_CASE(MOTOR_BLDC_UNDERVOLT);
        HANDLE_CASE(MOTOR_BLDC_OVERTEMP);
        HANDLE_CASE(MOTOR_BLDC_STARTUP_FAILED);
        HANDLE_CASE(MOTOR_BLDC_SPEEDSENSOR_FAILED);
        HANDLE_CASE(MOTOR_BLDC_OVERCURRENT);
        HANDLE_CASE(MOTOR_BLDC_DRIVER_ERROR);
        HANDLE_CASE(MOTOR_SPURIOUS_ERROR);
        HANDLE_CASE(MOTOR_UNKNOWN_ERROR);
        HANDLE_CASE(MOTOR_UNABLE_TO_MOVE);
        HANDLE_CASE(MOTOR_ILLEGAL_SPEED);
        HANDLE_CASE(MOTOR_ILLEGAL_RAMP_RATE);
        HANDLE_CASE(MOTOR_BAD_HOME);
        HANDLE_CASE(MOTOR_NOT_HOME);
        HANDLE_CASE(MOTOR_NOT_STOPPED);
        HANDLE_CASE(PLATE_LOCK_TIMEOUT);
        HANDLE_CASE(PLATE_LOCK_NOT_CLOSED);
        HANDLE_CASE(HEATER_THERMISTOR_A_DISCONNECTED);
        HANDLE_CASE(HEATER_THERMISTOR_A_SHORT);
        HANDLE_CASE(HEATER_THERMISTOR_A_OVERTEMP);
        HANDLE_CASE(HEATER_THERMISTOR_B_DISCONNECTED);
        HANDLE_CASE(HEATER_THERMISTOR_B_SHORT);
        HANDLE_CASE(HEATER_THERMISTOR_B_OVERTEMP);
        HANDLE_CASE(HEATER_THERMISTOR_BOARD_SHORT);
        HANDLE_CASE(HEATER_THERMISTOR_BOARD_OVERTEMP);
        HANDLE_CASE(HEATER_THERMISTOR_BOARD_DISCONNECTED);
        HANDLE_CASE(HEATER_HARDWARE_ERROR_LATCH);
        HANDLE_CASE(HEATER_CONSTANT_OUT_OF_RANGE);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
        HANDLE_CASE(SYSTEM_LED_I2C_NOT_READY);
        HANDLE_CASE(SYSTEM_LED_TRANSMIT_ERROR);
    }
    return UNKNOWN_ERROR;
}

auto errors::from_motor_error(uint16_t error_bitmap, MotorErrorOffset which)
    -> ErrorCode {
    if ((error_bitmap & (1 << static_cast<uint8_t>(which))) == 0) {
        return ErrorCode::NO_ERROR;
    }
    switch (which) {
        case MotorErrorOffset::FOC_DURATION:
            return ErrorCode::MOTOR_FOC_DURATION;
        case MotorErrorOffset::OVER_VOLT:
            return ErrorCode::MOTOR_BLDC_OVERVOLT;
        case MotorErrorOffset::UNDER_VOLT:
            return ErrorCode::MOTOR_BLDC_UNDERVOLT;
        case MotorErrorOffset::OVER_TEMP:
            return ErrorCode::MOTOR_BLDC_OVERTEMP;
        case MotorErrorOffset::START_UP:
            return ErrorCode::MOTOR_BLDC_STARTUP_FAILED;
        case MotorErrorOffset::SPEED_FDBK:
            return ErrorCode::MOTOR_BLDC_SPEEDSENSOR_FAILED;
        case MotorErrorOffset::OVERCURRENT:
            return ErrorCode::MOTOR_BLDC_OVERCURRENT;
        case MotorErrorOffset::SW_ERROR:
            return ErrorCode::MOTOR_BLDC_DRIVER_ERROR;
    }
    return ErrorCode::MOTOR_UNKNOWN_ERROR;
}
