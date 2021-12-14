// included in c++ and c files
#pragma once

/* size of transmission buffer for setting led */
#define SYSTEM_WIDE_TXBUFFERSIZE (size_t)(12)

/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24

/* Uncomment this to include aynchronous error messages */
//#define SYSTEM_ALLOW_ASYNC_ERRORS

typedef enum PeltierID {
    PELTIER_RIGHT,
    PELTIER_CENTER,
    PELTIER_LEFT,
    PELTIER_NUMBER
} PeltierID;

typedef enum PeltierDirection {
    PELTIER_COOLING,
    PELTIER_HEATING
} PeltierDirection;

enum PeltierSelection { LEFT, CENTER, RIGHT, ALL };

enum PidSelection { HEATER, PELTIERS, FANS };
