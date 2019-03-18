#ifndef THERMOCYCLER_H
#define THERMOCYCLER_H

#include <PID_v1.h>
#include "thermistorsadc.h"
#include "lid.h"
#include "peltiers.h"
#include "gcode.h"
#include "tc_timer.h"

/********* GCODE *********/
#define BAUDRATE 115200

/********* THERMISTORS *********/

#define THERMISTOR_VOLTAGE 1.5

/********* INDICATOR NEOPIXELS *********/

#include <Adafruit_NeoPixel_ZeroDMA.h>

#define NEO_PWR     4
#define NEO_PIN     A5
#define NUM_PIXELS  22

/********* FAN *********/

#define PIN_FAN_SINK_CTRL           A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE         2    // Heat sink fan

#define PIN_HEAT_PAD_CONTROL        A3
#define PIN_FAN_COVER               A2

double CURRENT_FAN_POWER = 0;

/********* TEMPERATURE PREDEFS *********/

#define TEMPERATURE_ROOM 23
#define TEMPERATURE_COVER_HOT 105

/********* PID: PLATE PELTIERS *********/

#define DEFAULT_PLATE_PID_TIME 20

#define PID_KP_PLATE_UP 0.05
#define PID_KI_PLATE_UP 0.01
#define PID_KD_PLATE_UP 0.0

#define PID_KP_PLATE_DOWN PID_KP_PLATE_UP
#define PID_KI_PLATE_DOWN PID_KI_PLATE_UP
#define PID_KD_PLATE_DOWN PID_KD_PLATE_UP

#define PID_STABILIZING_THRESH 5
#define PID_FAR_AWAY_THRESH 10
#define TARGET_TEMP_TOLERANCE 1.5   // Degree Celsius
double current_plate_kp = PID_KP_PLATE_UP;
double current_plate_ki = PID_KI_PLATE_UP;
double current_plate_kd = PID_KD_PLATE_UP;

bool MASTER_SET_A_TARGET = false;
bool auto_fan = true;
bool just_changed_temp = false;

double TEMPERATURE_SWING_PLATE = 0.5;
double TARGET_TEMPERATURE_PLATE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE_PLATE = TEMPERATURE_ROOM;

double TESTING_OFFSET_TEMP = TARGET_TEMPERATURE_PLATE;
double CURRENT_LEFT_PEL_TEMP = TEMPERATURE_ROOM;
double CURRENT_CENTER_PEL_TEMP = TEMPERATURE_ROOM;
double CURRENT_RIGHT_PEL_TEMP = TEMPERATURE_ROOM;
double TEMPERATURE_SWING_LEFT_PEL = 0.5;
double TEMPERATURE_SWING_CENTER_PEL = 0.5;
double TEMPERATURE_SWING_RIGHT_PEL = 0.5;

/********* PID: COVER HEAT PAD *********/

#define PID_KP_COVER 0.2
#define PID_KI_COVER 0.01
#define PID_KD_COVER 0.0

double TEMPERATURE_SWING_COVER = 0.5;
double TARGET_TEMPERATURE_COVER = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE_COVER = TEMPERATURE_ROOM;

bool COVER_SHOULD_BE_HOT = false;

/********* DEVICE INFO **********/

String device_serial = "";  // leave empty, this value is read from eeprom during setup()
String device_model = "";   // leave empty, this value is read from eeprom during setup()
String device_version = "v1.0.1";

/********* MISC GLOBALS *********/

#define USE_GCODES true
unsigned long plotter_timestamp = 0;
const int plotter_interval = 500;
bool running_from_script = false;
bool debug_print_mode = true;
bool running_graph = false;
bool zoom_mode = false;
/***************************************/
#endif
