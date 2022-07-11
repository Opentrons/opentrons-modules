// included in c++ and c files
#pragma once

/* size of transmission buffer for setting led */
#define SYSTEM_WIDE_TXBUFFERSIZE (size_t)(12)

/* size of array for setting serial number */
#define SYSTEM_WIDE_SERIAL_NUMBER_LENGTH 24

typedef enum { WHITE, RED, AMBER, BLUE, OFF , RED_WHITE, RED_AMBER, WHITE_AMBER } LED_COLOR;
typedef enum { SOLID_HOT, SOLID_HOLDING, PULSE, MODE_OFF } LED_MODE;
