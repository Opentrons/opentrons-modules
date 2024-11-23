// included in c++ and c files
#pragma once

typedef enum MotorID {
    MOTOR_Z,
    MOTOR_X,
    MOTOR_L,
} MotorID;

typedef enum TOFSensorID {
    TOF_Z,
    TOF_X,
} TOFSensorID;

enum MotorSelection { Z, X, L, ALL };

/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24
