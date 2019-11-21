#ifndef THERMOCYCLER_H
#define THERMOCYCLER_H

#include <PID_v1.h>
#include "thermistorsadc.h"
#include "lid.h"
#include "peltiers.h"
#include "gcode.h"
#include "tc_timer.h"
#include "fan.h"
#include "lights.h"
#include "eeprom.h"

/********* Version **********/
#ifdef TC_FW_VERSION
  #define FW_VERSION String(TC_FW_VERSION)
#else
  #error "No firmware version provided"
#endif

/********* GCODE *********/
#define BAUDRATE 115200

/********* THERMISTORS *********/

#define THERMISTOR_VOLTAGE 1.5

/* Thermistor offset values */
/* y = Ax1 + Bx2 + C
 * y: offset to plate temp
 * A, B, C: constants
 * x1: current heatsink temp
 * x2: target plate temp
 */
#define CONST_A_DEFAULT -0.026
#define CONST_B_DEFAULT 0.0231
#define CONST_C_DEFAULT 0.15

float const_a;
float const_b;
float const_c;

bool use_offset;
/********** HEAT PAD **********/

#if HW_VERSION >= 3
  #define PIN_HEAT_PAD_EN           25
#endif
#define PIN_HEAT_PAD_CONTROL        A3

/********* FAN *********/

#define PIN_FAN_COVER               A2
#define PIN_FAN_SINK_CTRL           A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE         2    // Heat sink fan
#define FAN_POWER_HIGH_2            0.8
#define FAN_POWER_HIGH_1            0.5
#define FAN_POWER_MED_1             0.3
#define FAN_POWER_MED_2             0.35
#define FAN_POWER_LOW               0.15
#define FAN_PWR_COLD_TARGET         0.7
#define FAN_PWR_RAMPING_DOWN        0.55
#define HEATSINK_P_CONSTANT         1.0

/****** FAN PID vars & constants ******/

double fan_pid_out = 0;
double heatsink_setpoint;
double current_heatsink_temp;

#define FAN_COLD_KP  0.2
#define FAN_COLD_KI  0.01
#define FAN_COLD_KD  0.05

#define FAN_HOT_KP  0.2
#define FAN_HOT_KI  0.0
#define FAN_HOT_KD  0.05

/********* TEMPERATURE PREDEFS *********/

#define TEMPERATURE_ROOM          23
#define TEMPERATURE_COVER_HOT     105
#define PELTIER_SAFE_TEMP_LIMIT   105
#define HEATSINK_SAFE_TEMP_LIMIT  85
#define HEATSINK_FAN_LO_TEMP      TEMPERATURE_ROOM
#define HEATSINK_FAN_MED_TEMP     31
#define HEATSINK_FAN_HI_TEMP_1    60
#define HEATSINK_FAN_HI_TEMP_2    68
#define HEATSINK_FAN_HI_TEMP_3    75
#define HEATSINK_FAN_OFF_TEMP     36
#define PELTIER_TEMP_DELTA        2
#define ACCEPTABLE_THERM_DIFF     2 // Difference allowed between any 2 plate thermistors (Celsius)

/****** OVERSHOOT EQUATION ******/
// y = mx + c
// y : overshoot
// m : constant (slope)
// x : volume (uL)
// c : constant

#define POS_OVERSHOOT_M 0.0105
#define POS_OVERSHOOT_C 1.0869
#define NEG_OVERSHOOT_M 0.0133
#define NEG_OVERSHOOT_C 0.4302

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
  #define PID_KP_PLATE_UP 0.2   // 0.2
  #define PID_KI_PLATE_UP 0.07  // 0.1
  #define PID_KD_PLATE_UP 0.0
#endif

#define PID_KP_PLATE_DOWN PID_KP_PLATE_UP
#define PID_KI_PLATE_DOWN PID_KI_PLATE_UP
#define PID_KD_PLATE_DOWN PID_KD_PLATE_UP

#define PID_STABILIZING_THRESH  5
#define PID_FAR_AWAY_THRESH     10
#define TARGET_TEMP_TOLERANCE   1.5   // Degree Celsius

#define OVERSHOOT_DURATION      10000  // millisec
#define DEFAULT_VOLUME          25     // uL
double current_plate_kp = PID_KP_PLATE_UP;
double current_plate_ki = PID_KI_PLATE_UP;
double current_plate_kd = PID_KD_PLATE_UP;

bool master_set_a_target = false;
bool auto_fan = true;
bool just_changed_temp = false;

double this_step_target_temp = TEMPERATURE_ROOM;
double temperature_swing_plate = 0.5;
double target_temperature_plate = TEMPERATURE_ROOM;
double current_temperature_plate = TEMPERATURE_ROOM;
uint16_t current_volume = DEFAULT_VOLUME;

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

String device_serial;
String device_model;

/********* Front Switch *********/

#if HW_VERSION >= 3
  #define PIN_FRONT_BUTTON_SW   23
  #define PIN_FRONT_BUTTON_LED  24
  #define LED_BRIGHTNESS        150
#endif

/********* MISC GLOBALS *********/

bool timer_started = false;
unsigned long overshoot_start_timestamp = 0;
#define DEBUG_PRINT_INTERVAL 2000   // millisec
#define ERROR_PRINT_INTERVAL 2000   // ms
unsigned long last_error_print = 0;
bool front_button_pressed = false;
unsigned long front_button_pressed_at = 0;
bool timer_interrupted = false;
uint8_t therm_read_state = 0;
bool gcode_debug_mode = false;
bool continuous_debug_stat_mode = false; // continuous_debug_stat_mode is not compatible with API
#if LID_TESTING
unsigned long gcode_rec_timestamp;
#endif
/***************************************/
#endif
