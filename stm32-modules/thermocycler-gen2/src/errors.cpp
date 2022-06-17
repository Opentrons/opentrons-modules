#include "thermocycler-gen2/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun\n";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full\n";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode\n";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full\n";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement\n";
const char* const THERMISTOR_HEATSINK_DISCONNECTED =
    "ERR201:Heatsink thermistor disconnected\n";
const char* const THERMISTOR_HEATSINK_SHORT =
    "ERR202:Heatsink thermistor shorted\n";
const char* const THERMISTOR_HEATSINK_OVERTEMP =
    "ERR203:Heatsink thermistor overtemp\n";
const char* const THERMISTOR_FRONT_RIGHT_DISCONNECTED =
    "ERR204:Front right thermistor disconnected\n";
const char* const THERMISTOR_FRONT_RIGHT_SHORT =
    "ERR205:Front right thermistor shorted\n";
const char* const THERMISTOR_FRONT_RIGHT_OVERTEMP =
    "ERR206:Front right thermistor overtemp\n";
const char* const THERMISTOR_FRONT_LEFT_DISCONNECTED =
    "ERR207:Front left thermistor disconnected\n";
const char* const THERMISTOR_FRONT_LEFT_SHORT =
    "ERR208:Front left thermistor shorted\n";
const char* const THERMISTOR_FRONT_LEFT_OVERTEMP =
    "ERR209:Front left thermistor overtemp\n";
const char* const THERMISTOR_FRONT_CENTER_DISCONNECTED =
    "ERR210:Front center thermistor disconnected\n";
const char* const THERMISTOR_FRONT_CENTER_SHORT =
    "ERR211:Front center thermistor shorted\n";
const char* const THERMISTOR_FRONT_CENTER_OVERTEMP =
    "ERR212:Front center thermistor overtemp\n";
const char* const THERMISTOR_BACK_RIGHT_DISCONNECTED =
    "ERR213:Back right thermistor disconnected\n";
const char* const THERMISTOR_BACK_RIGHT_SHORT =
    "ERR214:Back right thermistor shorted\n";
const char* const THERMISTOR_BACK_RIGHT_OVERTEMP =
    "ERR215:Back right thermistor overtemp\n";
const char* const THERMISTOR_BACK_LEFT_DISCONNECTED =
    "ERR216:Back left thermistor disconnected\n";
const char* const THERMISTOR_BACK_LEFT_SHORT =
    "ERR217:Back left thermistor shorted\n";
const char* const THERMISTOR_BACK_LEFT_OVERTEMP =
    "ERR218:Back left thermistor overtemp\n";
const char* const THERMISTOR_BACK_CENTER_DISCONNECTED =
    "ERR219:Back center thermistor disconnected\n";
const char* const THERMISTOR_BACK_CENTER_SHORT =
    "ERR220:Back center thermistor shorted\n";
const char* const THERMISTOR_BACK_CENTER_OVERTEMP =
    "ERR221:Back center thermistor overtemp\n";
const char* const THERMISTOR_LID_DISCONNECTED =
    "ERR222:Lid thermistor disconnected\n";
const char* const THERMISTOR_LID_SHORT = "ERR223:Lid thermistor shorted\n";
const char* const THERMISTOR_LID_OVERTEMP = "ERR224:Lid thermistor overtemp\n";
const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format\n";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout\n";
const char* const SYSTEM_EEPROM_ERROR =
    "ERR303:system:EEPROM communication error\n";
const char* const THERMAL_PLATE_BUSY = "ERR401:thermal:Thermal plate busy\n";
const char* const THERMAL_PELTIER_ERROR =
    "ERR402:thermal:Could not activate peltier\n";
const char* const THERMAL_HEATSINK_FAN_ERROR =
    "ERR403:thermal:Could not control heatsink fan\n";
const char* const THERMAL_LID_BUSY = "ERR404:thermal:Lid heater is busy\n";
const char* const THERMAL_HEATER_ERROR =
    "ERR405:thermal:Error controlling lid heater\n";
const char* const THERMAL_CONSTANT_OUT_OF_RANGE =
    "ERR406:thermal:PID constant(s) out of range\n";
const char* const THERMAL_TARGET_BAD =
    "ERR407:thermal:Invalid target temperature\n";
const char* const THERMAL_DRIFT =
    "ERR408:thermal:Thermal drift of more than 4C\n";
const char* const LID_MOTOR_BUSY = "ERR501:lid:Lid motor busy\n";
const char* const LID_MOTOR_FAULT = "EERR502:lid:Lid motor fault\n";
const char* const SEAL_MOTOR_SPI_ERROR = "ERR503:seal:SPI error\n";
const char* const SEAL_MOTOR_BUSY = "EERR504:seal:Seal motor busy\n";
const char* const SEAL_MOTOR_FAULT = "ERR505:seal:Seal motor fault\n";
const char* const SEAL_MOTOR_STALL = "ERR5006:seal:Seal motor stall event\n";
const char* const LID_CLOSED = "ERR507:lid:Lid must be opened\n";
const char* const SEAL_MOTOR_SWITCH =
    "ERR508:seal:Seal switch should not be engaged\n";

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
        HANDLE_CASE(THERMISTOR_HEATSINK_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_HEATSINK_SHORT);
        HANDLE_CASE(THERMISTOR_HEATSINK_OVERTEMP);
        HANDLE_CASE(THERMISTOR_FRONT_RIGHT_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_FRONT_RIGHT_SHORT);
        HANDLE_CASE(THERMISTOR_FRONT_RIGHT_OVERTEMP);
        HANDLE_CASE(THERMISTOR_FRONT_LEFT_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_FRONT_LEFT_SHORT);
        HANDLE_CASE(THERMISTOR_FRONT_LEFT_OVERTEMP);
        HANDLE_CASE(THERMISTOR_FRONT_CENTER_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_FRONT_CENTER_SHORT);
        HANDLE_CASE(THERMISTOR_FRONT_CENTER_OVERTEMP);
        HANDLE_CASE(THERMISTOR_BACK_RIGHT_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_BACK_RIGHT_SHORT);
        HANDLE_CASE(THERMISTOR_BACK_RIGHT_OVERTEMP);
        HANDLE_CASE(THERMISTOR_BACK_LEFT_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_BACK_LEFT_SHORT);
        HANDLE_CASE(THERMISTOR_BACK_LEFT_OVERTEMP);
        HANDLE_CASE(THERMISTOR_BACK_CENTER_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_BACK_CENTER_SHORT);
        HANDLE_CASE(THERMISTOR_BACK_CENTER_OVERTEMP);
        HANDLE_CASE(THERMISTOR_LID_DISCONNECTED);
        HANDLE_CASE(THERMISTOR_LID_SHORT);
        HANDLE_CASE(THERMISTOR_LID_OVERTEMP);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
        HANDLE_CASE(SYSTEM_EEPROM_ERROR);
        HANDLE_CASE(THERMAL_PLATE_BUSY);
        HANDLE_CASE(THERMAL_PELTIER_ERROR);
        HANDLE_CASE(THERMAL_HEATSINK_FAN_ERROR);
        HANDLE_CASE(THERMAL_LID_BUSY);
        HANDLE_CASE(THERMAL_HEATER_ERROR);
        HANDLE_CASE(THERMAL_CONSTANT_OUT_OF_RANGE);
        HANDLE_CASE(THERMAL_TARGET_BAD);
        HANDLE_CASE(THERMAL_DRIFT);
        HANDLE_CASE(LID_MOTOR_BUSY);
        HANDLE_CASE(LID_MOTOR_FAULT);
        HANDLE_CASE(SEAL_MOTOR_SPI_ERROR);
        HANDLE_CASE(SEAL_MOTOR_BUSY);
        HANDLE_CASE(SEAL_MOTOR_FAULT);
        HANDLE_CASE(SEAL_MOTOR_STALL);
        HANDLE_CASE(LID_CLOSED);
        HANDLE_CASE(SEAL_MOTOR_SWITCH);
    }
    return UNKNOWN_ERROR;
}
