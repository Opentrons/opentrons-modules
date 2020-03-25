#ifndef Magdeck_h
#define Magdeck_h

#include <Arduino.h>

/********* Version **********/
#ifdef MD_FW_VERSION
  #define FW_VERSION String(MD_FW_VERSION)
#else
  #error "No firmware version provided"
#endif

#define MODEL_VER_TEMPLATE "mag_deck_v"
#define MODEL_VER_TEMPLATE_LEN sizeof(MODEL_VER_TEMPLATE) - 1

#define ADDRESS_DIGIPOT 0x2D  // 7-bit address

#define MOTOR_ENGAGE_PIN 10
#define MOTOR_DIRECTION_PIN 9
#define MOTOR_STEP_PIN 6
#define LED_UP_PIN 13
#define LED_DOWN_PIN 5
#define ENDSTOP_PIN A5
#define ENDSTOP_PIN_TOP A4
#define TONE_PIN 11

#define DIRECTION_DOWN HIGH
#define DIRECTION_UP LOW

#define ENDSTOP_TRIGGERED_STATE LOW

#define CURRENT_TO_BYTES_FACTOR 114

#endif