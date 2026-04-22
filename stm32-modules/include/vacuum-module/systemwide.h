// included in c++ and c files
#pragma once

typedef enum PressureSensorID {
    ABS_PRESSURE_A = 0,
    ABS_PRESSURE_B,
    ATM_PRESSURE,
} PressureSensorID;

typedef enum PressureSensorState {
    DISABLED = 0,
    INITIALIZING,
    IDLE,
    MEASURING,
    SENSOR_ERROR,
} PressureSensorState;

typedef enum PressureSensorError {
    NO_ERROR = 0,
    DRIVER_INIT_ERROR,
    SENSOR_BUSY_ERROR,
    MATH_SATURATION_ERROR,
    UNKNOWN_ERROR,
} PressureSensorError;

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

typedef enum VentState { CLOSED = 0, OPENED = 1 } VentState;

/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24

/* Max size of array for debug message */
#define DEBUG_MESSAGE_LENGTH 100

typedef void* TaskHandle;

#define MIN_PWM 0
#define MAX_PWM 100
#define MIN_RPM 0.0f
#define MAX_RPM 3500.0f
