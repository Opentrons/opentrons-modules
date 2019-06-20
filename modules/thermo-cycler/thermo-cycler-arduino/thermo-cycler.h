#ifndef THERMOCYCLER_H
#define THERMOCYCLER_H

#include <PID_v1.h>
#include "thermistorsadc.h"
#include "lid.h"
#include "peltiers.h"
#include "gcode.h"
#include "tc_timer.h"
#include "fan.h"

/********* Versions **********/
/* Version guidelines: */
#define FW_VERSION "Beta3.0"

/********* GCODE *********/
#define BAUDRATE 115200

/********* THERMISTORS *********/

#define THERMISTOR_VOLTAGE 1.5

/********* INDICATOR NEOPIXELS *********/

#include <Adafruit_NeoPixel_ZeroDMA.h>

#define NEO_PWR     4
#define NEO_PIN     A5
#define NUM_PIXELS  16

/********** HEAT PAD **********/

#if HW_VERSION >= 3
  #define PIN_HEAT_PAD_EN           25
#endif
#define PIN_HEAT_PAD_CONTROL        A3

/********* FAN *********/

#define PIN_FAN_COVER               A2
#define PIN_FAN_SINK_CTRL           A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE         2    // Heat sink fan
#define FAN_POWER_HIGH              0.8
#define FAN_POWER_LOW               0.2

/********* TEMPERATURE PREDEFS *********/

#define TEMPERATURE_ROOM 26
#define TEMPERATURE_COVER_HOT 105

/********* PID: PLATE PELTIERS *********/
// NOTE: temp_probes.update takes 136-137ms while rest of the loop takes 0-1ms.
//       Using 135ms sample time guarantees that the PID value is computed every
//       137ms with a very minute error in computation due to 1-2ms difference.
//       If <135ms, PID computation error will increase
//       If >=137ms, the compute will miss the window before temp_probes.update
//       is called again; which makes the next PID compute 2*137ms away
#if OLD_PID_INTERVAL
  #define DEFAULT_PLATE_PID_TIME 100
#else
  #define DEFAULT_PLATE_PID_TIME 135
#endif

#if HFQ_PWM
  #define PID_KP_PLATE_UP 0.1   //0.11 dampens the first spike but takes slightly longer to stabilize
  #define PID_KI_PLATE_UP 0.03
  #define PID_KD_PLATE_UP 0.0
#else
  #define PID_KP_PLATE_UP 0.2   // 0.16
  #define PID_KI_PLATE_UP 0.1   // 0.07
  #define PID_KD_PLATE_UP 0.0
#endif

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

/********* Front Switch *********/

#if HW_VERSION >= 3
  #define PIN_FRONT_BUTTON_SW   23
  #define PIN_FRONT_BUTTON_LED  24
  #define LED_BRIGHTNESS        150
#endif

/********* MISC GLOBALS *********/

#define DEBUG_PRINT_INTERVAL 2000  // millisec
unsigned long plotter_timestamp = 0;
const int plotter_interval = 500;
bool front_button_pressed = false;
unsigned long front_button_pressed_at = 0;
bool running_from_script = false;
bool debug_print_mode = true;
bool gcode_debug_mode = false;  // Debug mode is not compatible with API
bool continuous_debug_stat_mode = false;
bool running_graph = false;
bool zoom_mode = false;
#if LID_TESTING
unsigned long gcode_rec_timestamp;
#endif
/***************************************/
#endif
