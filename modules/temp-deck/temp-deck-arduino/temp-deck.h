#ifndef TEMP_DECK_H
#define TEMP_DECK_H

#include <Arduino.h>
#include <avr/wdt.h>

/*
  PID library written by github user br3ttb:
  can be found at:
  https://github.com/br3ttb/Arduino-PID-Library
*/
#include <PID_v1.h>

/*
  the below "lights.h" class uses Adafruit's 16-channel PWM I2C driver library
  can be found at:
  https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
*/
#include "lights.h"

// load in some custom classes for this device
#include "memory.h"
#include "peltiers.h"
#include "thermistor.h"
#include "gcode.h"

/********* Version **********/
#ifdef TD_FW_VERSION
  #define FW_VERSION String(TD_FW_VERSION)
#else
  #error "No firmware version provided"
#endif

#define MODEL_VER_TEMPLATE "temp_deck_v"
#define MODEL_VER_TEMPLATE_LEN sizeof(MODEL_VER_TEMPLATE) - 1
#define SERIAL_VER_TEMPLATE "TDV03P2018"
#define SERIAL_VER_TEMPLATE_LEN sizeof(SERIAL_VER_TEMPLATE) - 1

#define PIN_BUZZER 11  // a piezo buzzer we can use tone() with
#define PIN_FAN 9      // blower-fan controlled by simple PWM analogWrite()

// the maximum temperatures the device can target
// this is limitted mostly by the 2-digit temperature display
#define TEMPERATURE_MAX 99
#define TEMPERATURE_MIN -9

// some temperature zones to help decide on states
#define TEMPERATURE_ROOM 23
#define TEMPERATURE_FAN_CUTOFF_COLD TEMPERATURE_ROOM
#define TEMPERATURE_FAN_CUTOFF_HOT 35

#define TEMPERATURE_BURN 55
#define STABILIZING_ZONE 0.5
#define ERROR_PRINT_INTERVAL 2000 // millis
bool reached_unsafe_temp = false;

// values used to scale the thermistors temperature
// to more accurately reflect the temperature of the top-plate
#define THERMISTOR_OFFSET_LOW_TEMP 5.25
#define THERMISTOR_OFFSET_LOW_VALUE -0.1
#define THERMISTOR_OFFSET_HIGH_TEMP 95
#define THERMISTOR_OFFSET_HIGH_VALUE -1.4
const float THERMISTOR_OFFSET_HIGH_TEMP_DIFF = THERMISTOR_OFFSET_HIGH_TEMP - TEMPERATURE_ROOM;
const float THERMISTOR_OFFSET_LOW_TEMP_DIFF = TEMPERATURE_ROOM - THERMISTOR_OFFSET_LOW_TEMP;

// the intensities of the fan (0.0-1.0)
#define FAN_HIGH 1.0
#define FAN_LOW 0.3
#define FAN_OFF 0.0

// some model versions 3.0+ & 4.0+ have a different fan that requires on/off cycles (not PWM)
#define MAX_FAN_OFF_TIME    4000
#define FAN_V3_V4_LOW_ON_PC 0.75

// some model v4 fans are indeed PWM. We need to keep their power low enough while
// pulsing so they are able to turn off momentarily.
#define FAN_V3_V4_LOW_PWR   100   // PWM value (= 39%)
#define FAN_V3_V4_HI_PWR    214   // PWM value (214 = 85%)

unsigned long fan_on_time = 0;
unsigned long fan_off_time = MAX_FAN_OFF_TIME;
unsigned long fan_timestamp = 0;
bool is_fan_on = false;
bool is_v3_v4_fan = false;

// the "Kd" of the PID never changes in our setup
// (works according to testing so far...)
#define DEFAULT_PID_KD 0.0

// the "Kp" and "Ki" value for whenever the target is BELOW current temperature
// stays constant for all target temperature
// (works according to testing so far...)
#define DOWN_PID_KP 0.38
#define DOWN_PID_KI 0.0275

// the "Kp" and "Ki" value for whenever the target is ABOVE current temperature
// linear interpolation between LOW and HIGH target values
// (works according to testing so far...)
#define UP_PID_LOW_TEMP 40
#define UP_PID_HIGH_TEMP 100
#define UP_PID_KP_AT_LOW_TEMP 0.17    // "Kp" when target is UP_PID_LOW_TEMP
#define UP_PID_KP_AT_HIGH_TEMP 0.26   // "Kp" when target is UP_PID_HIGH_TEMP
#define UP_PID_KI_AT_LOW_TEMP 0.012   // "Ki" when target is UP_PID_LOW_TEMP
#define UP_PID_KI_AT_HIGH_TEMP 0.0225 // "Ki" when target is UP_PID_HIGH_TEMP

// the "Kp" and "Ki" value for whenever the target is ABOVE current temperature
// BUT also in the cold zone (<15deg)
#define UP_PID_KP_IN_COLD_ZONE 0.21
#define UP_PID_KI_IN_COLD_ZONE 0.015

/* Thermistor offset values */
/* y = mx + b
 * y: offset to plate temp
 * m, b: constants
 * x: target temp
 */
#define CONST_M_DEFAULT 0.01535
#define CONST_B_DEFAULT -0.34

bool use_target_dependent_offset = false;

// to use with the Arduino IDE's "Serial Plotter" graphing tool
// (very, very useful when testing PID tuning values)
// uncomment below line to print temperature and PID information

//#define DEBUG_PLOTTER_ENABLED

#ifdef DEBUG_PLOTTER_ENABLED
#define DEBUG_PLOTTER_INTERVAL 250
unsigned long debug_plotter_timestamp = 0;
#endif

// when a new target-temperature is set, and the peltiers need to shift directions suddenly
// that is the moment when they draw the most current (4.3 amps). If the fan is on HIGH
// at the same time (>2.0 amps) then we are over-working our 6.1 amp power supply.
// So, these variables are used whenever a NEW target temperature is set. They do the following:
//    1) turn off both the peltiers and fan, and wait until the fan has completely shut down (do nothing until then)
//    3) once fan is off, turn on the peltiers to the correct state (potentially drawing 4.3 amps!)
//    4) after the the peltiers' current has dropping some, turn the fan back on

// uncomment to turn off system after setting the temperature
#define CONSERVE_POWER_ON_SET_TARGET

#ifdef CONSERVE_POWER_ON_SET_TARGET
unsigned long SET_TEMPERATURE_TIMESTAMP = 0;
#define millis_till_fan_turns_off 2000 // how long to wait before #1 and #2 from the list above
#define millis_till_peltiers_drop_current 2000 // how long to wait before #2 and #3 from the list above
#endif

// -1.0 is full cold peltiers, +1.0 is full hot peltiers, can be between the two
double TEMPERATURE_SWING;
double TARGET_TEMPERATURE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE = TEMPERATURE_ROOM;
bool MASTER_SET_A_TARGET = false;

unsigned long start_bootloader_timestamp = 0;
const int start_bootloader_timeout = 1000;

#endif
