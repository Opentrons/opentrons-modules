#ifndef THERMOCYCLER_H
#define THERMOCYCLER_H

#include <PID_v1.h>
#include "thermistorsadc.h"
#include "lid.h"
#include "peltiers.h"
#include "gcode.h"
#include "tc_timer.h"
#include "fan.h"

/********* GCODE *********/
#define BAUDRATE 115200

/********* THERMISTORS *********/

#define THERMISTOR_VOLTAGE 1.5

/********* INDICATOR NEOPIXELS *********/

#include <Adafruit_NeoPixel_ZeroDMA.h>

#define NEO_PWR     4
#define NEO_PIN     A5
#define NUM_PIXELS  22

/********** HEAT PAD **********/

#define PIN_HEAT_PAD_CONTROL        A3

/********* FAN *********/

#define PIN_FAN_COVER               A2
#define PIN_FAN_SINK_CTRL           A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE         2    // Heat sink fan
#define FAN_POWER_HIGH              0.8
#define FAN_POWER_LOW               0.2

/********* TEMPERATURE PREDEFS *********/

#define TEMPERATURE_ROOM 23
#define TEMPERATURE_COVER_HOT 105

/********* PID: PLATE PELTIERS *********/

#define DEFAULT_PLATE_PID_TIME 100

#define PID_KP_PLATE_UP 0.2
#define PID_KI_PLATE_UP 0.1
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

bool master_set_a_target = false;
bool auto_fan = true;
bool just_changed_temp = false;

double temperature_swing_plate = 0.5;
double target_temperature_plate = TEMPERATURE_ROOM;
double current_temperature_plate = TEMPERATURE_ROOM;

double testing_offset_temp = target_temperature_plate;
double current_left_pel_temp = TEMPERATURE_ROOM;
double current_center_pel_temp = TEMPERATURE_ROOM;
double current_right_pel_temp = TEMPERATURE_ROOM;
double temperature_swing_left_pel = 0.5;
double temperature_swing_center_pel = 0.5;
double temperature_swing_right_pel = 0.5;

/********* PID: COVER HEAT PAD *********/

#define PID_KP_COVER 0.2
#define PID_KI_COVER 0.01
#define PID_KD_COVER 0.0

double temperature_swing_cover = 0.5;
double target_temperature_cover = TEMPERATURE_ROOM;
double current_temperature_cover = TEMPERATURE_ROOM;

bool cover_should_be_hot = false;

/********* DEVICE INFO **********/

String device_serial = "dummySerial";  // TODO: remove stub, later leave empty, this value is read from eeprom during setup()
String device_model = "dummyModel";   // TODO: remove stub, later leave empty, this value is read from eeprom during setup()
String device_version = "v1.0.1";

/********* MISC GLOBALS *********/

unsigned long plotter_timestamp = 0;
const int plotter_interval = 500;
bool running_from_script = false;
bool debug_print_mode = true;
bool running_graph = false;
bool zoom_mode = false;
/***************************************/
#endif
