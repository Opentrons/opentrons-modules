// included in c++ and c files
#pragma once

typedef enum MotorID {
    MOTOR_Z,
    MOTOR_X,
    MOTOR_L,
} MotorID;

typedef enum TOFSensorID {
    TOF_NONE,
    TOF_Z,
    TOF_X,
} TOFSensorID;

typedef enum TOFSensorMode {
    UNKNOWN = 0x00,
    MEASURE = 0x03,
    BOOTLOADER = 0x80,
} TOFSensorMode;

typedef enum TOFSensorState {
    DISABLED = 0,
    INITIALIZING,
    IDLE,
    MEASURING,
    TOF_ERROR,
} TOFSensorState;

typedef enum TOFMeasurementKind {
    HISTOGRAM = 0,
    MEASUREMENT = 1,
} TOFMeasurementKind;

typedef enum TOFActiveRange {
    NOT_SUPPORTED = 0,
    SHORT_RANGE = 0x6E,
    LONG_RANGE = 0x6F,
} TOFActiveRange;

typedef enum TOFSpadMapID {
    // 3x3 normal mode 33°x32° FoV
    SPAD_MAP_ID_1 = 1,
    // 3x3 macro 1 mode 33°x47° FoV off center
    SPAD_MAP_ID_2 = 2,
    // 3x3 macro 2 mode 33°x47° FoV
    SPAD_MAP_ID_3 = 3,
    // 3x3 wide mode 41°x52° FoV
    SPAD_MAP_ID_6 = 6,
    // 3x3 mode 33°x32° FoV, checkerboard
    SPAD_MAP_ID_11 = 11,
    // 3x3 mode 33°x32° FoV, inverted checkerboard
    SPAD_MAP_ID_12 = 12,
    // User defined mode, single measurement mode
    SPAD_MAP_ID_14 = 14,
} TOFSpadMapID;

typedef enum StatusBarID {
    Internal = 0,
    External,
} StatusBarID;

typedef enum StatusBarColor {
    White = 0,
    Red,
    Green,
    Blue,
    Yellow,
} StatusBarColor;

typedef enum StatusBarPattern {
    Static = 0,
    Flash,
    Pulse,
    Confirm,
} StatusBarPattern;

enum MotorSelection { Z, X, L, ALL };

/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24
