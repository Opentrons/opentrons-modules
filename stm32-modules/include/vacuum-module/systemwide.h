// included in c++ and c files
#pragma once

typedef enum PressureSensorID {
    ABS_PRESSURE_A = 0,
    ABS_PRESSURE_B,
    ATM_PRESSURE,
} PressureSensorID;

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

typedef enum VacuumPressureSensorId {
    SensorA,  // Sensor A
    SensorB,  // Sensor B
} VacuumPressureSensorId;
/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24
