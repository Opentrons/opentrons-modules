/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

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


#define PIN_BUZZER 11  // a piezo buzzer we can use tone() with
#define PIN_FAN 9      // blower-fan controlled by simple PWM analogWrite()

// the maximum temperatures the device can target
// this is limitted mostly by the 2-digit temperature display
#define TEMPERATURE_MAX 99
#define TEMPERATURE_MIN -9

// some temperature zones to help decide on states
#define TEMPERATURE_FAN_CUTOFF_COLD 15
#define TEMPERATURE_FAN_CUTOFF_HOT 35
#define TEMPERATURE_ROOM 25

#define TEMPERATURE_BURN 55
#define STABILIZING_ZONE 0.5

// the intensities of the fan (0.0-1.0)
#define FAN_HIGH 1.0
#define FAN_LOW 0.4
#define FAN_OFF 0.0

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
//#define CONSERVE_POWER_ON_SET_TARGET

#ifdef CONSERVE_POWER_ON_SET_TARGET
unsigned long SET_TEMPERATURE_TIMESTAMP = 0;
const unsigned long millis_till_fan_turns_off = 2000; // how long to wait before #1 and #2 from the list above
const unsigned long millis_till_peltiers_drop_current = 2000; // how long to wait before #2 and #3 from the list above
#endif

// -1.0 is full cold peltiers, +1.0 is full hot peltiers, can be between the two
double TEMPERATURE_SWING;
double TARGET_TEMPERATURE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE = TEMPERATURE_ROOM;
bool MASTER_SET_A_TARGET = false;

double pid_Kp = DOWN_PID_KP;
double pid_Ki = DOWN_PID_KI;
double pid_Kd = DEFAULT_PID_KD;

PID myPID(&CURRENT_TEMPERATURE, &TEMPERATURE_SWING, &TARGET_TEMPERATURE, pid_Kp, pid_Ki, pid_Kd, P_ON_M, DIRECT);

String device_serial = "";  // leave empty, this value is read from eeprom during setup()
String device_model = "";   // leave empty, this value is read from eeprom during setup()
String device_version = "v1.0.0-beta1";

Lights lights = Lights();  // controls 2-digit 7-segment numbers, and the RGBW color bar
Peltiers peltiers = Peltiers();  // 2 peltiers wired in series (-1.0<->1.0 controls polarity and intensity)
Thermistor thermistor = Thermistor();  // uses thermistor to read calculate the top-plate's temperature
Gcode gcode = Gcode();  // reads in serial data to parse command and issue reponses
Memory memory = Memory();  // reads from EEPROM to find device's unique serial, and model number

// some variables to help initiate the bootloader some time after it has been commanded to start
unsigned long now;

unsigned long start_bootloader_timestamp = 0;
const int start_bootloader_timeout = 1000;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool stabilizing() {
  return abs(TARGET_TEMPERATURE - CURRENT_TEMPERATURE) < STABILIZING_ZONE;
}

bool moving_down() {
  return CURRENT_TEMPERATURE - TARGET_TEMPERATURE > STABILIZING_ZONE;
}

bool moving_up() {
  return TARGET_TEMPERATURE - CURRENT_TEMPERATURE > STABILIZING_ZONE;
}

bool burning_hot() {
  return CURRENT_TEMPERATURE > TEMPERATURE_BURN;
}

bool cold_zone() {
  return CURRENT_TEMPERATURE < TEMPERATURE_FAN_CUTOFF_COLD;
}

bool middle_zone() {
  return CURRENT_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_COLD && CURRENT_TEMPERATURE < TEMPERATURE_FAN_CUTOFF_HOT;
}

bool hot_zone() {
  return CURRENT_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_HOT;
}

bool target_cold_zone() {
  return TARGET_TEMPERATURE < TEMPERATURE_FAN_CUTOFF_COLD;
}

bool target_middle_zone() {
  return TARGET_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_COLD && TARGET_TEMPERATURE < TEMPERATURE_FAN_CUTOFF_HOT;
}

bool target_hot_zone() {
  return TARGET_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_HOT;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_target_temperature(double target_temp){
  if (target_temp < TEMPERATURE_MIN) {
    target_temp = TEMPERATURE_MIN;
    gcode.print_warning(
      F("Target temperature too low, setting to TEMPERATURE_MIN degrees"));
  }
  if (target_temp > TEMPERATURE_MAX) {
    target_temp = TEMPERATURE_MAX;
    gcode.print_warning(
      F("Target temperature too high, setting to TEMPERATURE_MAX degrees"));
  }
  TARGET_TEMPERATURE = target_temp;
  lights.flash_on();

#ifdef CONSERVE_POWER_ON_SET_TARGET
  SET_TEMPERATURE_TIMESTAMP = millis();
#endif

  adjust_pid_on_new_target();
}

void turn_off_target() {
  set_target_temperature(TEMPERATURE_ROOM);
  MASTER_SET_A_TARGET = false;
  lights.flash_off();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_power(float percentage){
  percentage = constrain(percentage, 0.0, 1.0);
  analogWrite(PIN_FAN, int(percentage * 255.0));
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_peltiers_from_pid() {
  if (TEMPERATURE_SWING < 0) {
    peltiers.set_cold_percentage(abs(TEMPERATURE_SWING));
  }
  else {
    peltiers.set_hot_percentage(TEMPERATURE_SWING);
  }
}

void adjust_pid_on_new_target() {
  pid_Kp = DOWN_PID_KP;
  pid_Ki = DOWN_PID_KI;
  pid_Kd = DEFAULT_PID_KD;

  if (moving_up()) {
    if (target_cold_zone()) {
      pid_Kp = UP_PID_KP_IN_COLD_ZONE;
      pid_Ki = UP_PID_KI_IN_COLD_ZONE;
    }
    else if (TARGET_TEMPERATURE <= UP_PID_LOW_TEMP) {
      pid_Kp = UP_PID_KP_AT_LOW_TEMP;
      pid_Ki = UP_PID_KI_AT_LOW_TEMP;
    }
    else if (TARGET_TEMPERATURE >= UP_PID_HIGH_TEMP) {
      pid_Kp = UP_PID_KP_AT_HIGH_TEMP;
      pid_Ki = UP_PID_KI_AT_HIGH_TEMP;
    }
    else {
      // linear function between the MIN and MAX Kp and Ki values when moving up
      float scaler = (TARGET_TEMPERATURE - UP_PID_LOW_TEMP) / (UP_PID_HIGH_TEMP - UP_PID_LOW_TEMP);
      pid_Kp = UP_PID_KP_AT_LOW_TEMP + (scaler * (UP_PID_KP_AT_HIGH_TEMP - UP_PID_KP_AT_LOW_TEMP));
      pid_Ki = UP_PID_KI_AT_LOW_TEMP + (scaler * (UP_PID_KI_AT_HIGH_TEMP - UP_PID_KI_AT_LOW_TEMP));
    }
  }

  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd, P_ON_M);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void stabilize_to_target_temp(bool set_fan=true){

#ifdef CONSERVE_POWER_ON_SET_TARGET
  // first, avoid drawing too much current when the target was just changed
  now = millis();
  if (SET_TEMPERATURE_TIMESTAMP > now) SET_TEMPERATURE_TIMESTAMP = now;  // handle rollover
  unsigned long end_time = SET_TEMPERATURE_TIMESTAMP + millis_till_fan_turns_off;
  if (end_time > now) {
    peltiers.disable_peltiers();
    set_fan_power(FAN_OFF);
    return;  // EXIT the function, do nothing, wait
  }
  end_time += millis_till_peltiers_drop_current;
  if (end_time > now) {
    set_fan = false;
  }
#endif

  // seconds, set the fan to the correct intensity
  if (set_fan == false) {
    set_fan_power(FAN_OFF);
  }
  else if (target_cold_zone()) {
    set_fan_power(FAN_HIGH);
  }
  else if (target_middle_zone() && moving_down()) {
    set_fan_power(FAN_HIGH);
  }
  else {
    set_fan_power(FAN_LOW);
  }

  // third, update the
  set_peltiers_from_pid();
}

void stabilize_to_room_temp(bool set_fan=true) {
  if (burning_hot()) {
    set_peltiers_from_pid();
    if (set_fan) {
      set_fan_power(FAN_LOW);
    }
  }
  else {
    peltiers.disable_peltiers();
    set_fan_power(FAN_OFF);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_led_display(boolean debounce=true){
  // round to closest whole-number temperature
  lights.display_number(CURRENT_TEMPERATURE + 0.5, debounce);
  // if we are targetting, and close to the value, stop flashing
  if (!MASTER_SET_A_TARGET || stabilizing()) {
    lights.flash_off();
  }
  // targetting and not close to value, continue flashing
  else {
    lights.flash_on();
  }

  // set the color-bar color depending on if we're targing hot or cold temperatures
  if (!MASTER_SET_A_TARGET) {
    lights.set_color_bar(0, 0, 0, 1);  // white
  }
  else if (TARGET_TEMPERATURE < TEMPERATURE_ROOM) {
    lights.set_color_bar(0, 0, 1, 0);  // blue
  }
  else {
    lights.set_color_bar(1, 0, 0, 0);  // red
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void activate_bootloader(){
  // Method 1: Uses a WDT reset to enter bootloader.
  // Works on the modified Caterina bootloader that allows 
  // bootloader access after a WDT reset
  // -----------------------------------------------------------------
  wdt_enable(WDTO_15MS);  //Timeout
  unsigned long timerStart = millis();
  while(millis() - timerStart < 25){
    //Wait out until WD times out
  }
  // Should never get here but in case it does because 
  // WDT failed to start or timeout..
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void print_temperature() {
  if (MASTER_SET_A_TARGET) {
    gcode.print_targetting_temperature(
      TARGET_TEMPERATURE,
      CURRENT_TEMPERATURE
    );
  }
  else {
    gcode.print_stablizing_temperature(
      CURRENT_TEMPERATURE
    );
  }
}

void read_gcode(){
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_NO_CODE:
          break;
        case GCODE_GET_TEMP:
          print_temperature();
          break;
        case GCODE_SET_TEMP:
          if (gcode.read_number('S')) {
            set_target_temperature(gcode.parsed_number);
            MASTER_SET_A_TARGET = true;
          }
          if (gcode.read_number('P')) {
            pid_Kp = gcode.parsed_number;
          }
          if (gcode.read_number('I')) {
            pid_Ki = gcode.parsed_number;
          }
          if (gcode.read_number('D')) {
            pid_Kd = gcode.parsed_number;
          }
          myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd, P_ON_M);
          break;
        case GCODE_DISENGAGE:
          turn_off_target();
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, device_version);
          break;
        case GCODE_DFU:
          gcode.print_warning(F("Restarting and entering bootloader..."));
          activate_bootloader();

          break;
        default:
          break;
      }
    }
    gcode.send_ack();
  }
}

void turn_off_serial_lights() {
  // disable the Tx/Rx LED output (too distracting)
  TXLED1;
  RXLED1;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

  turn_off_serial_lights();

  wdt_disable();          /* Disable watchdog if enabled by bootloader/fuses */

  gcode.setup(115200);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);

  set_fan_power(FAN_OFF);

  // 250ms is the PWM cycle-time (up/down) for the peltiers
  // it shouldn't be shorter than 100ms, as peltiers are not designed to cycle that fast
  peltiers.setup_peltiers(250);
  peltiers.disable_peltiers();

  memory.read_serial(device_serial);
  memory.read_model(device_model);

  lights.setup_lights();
  lights.set_numbers_brightness(0.25);
  lights.set_color_bar_brightness(0.5);

  // make sure we start with an averaged plate temperatures
  while (!thermistor.update()) {}
  CURRENT_TEMPERATURE = thermistor.plate_temperature();
  TARGET_TEMPERATURE = TEMPERATURE_ROOM;

  // setup PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(500);  // since peltier's update at 250ms, PID can be slower
  myPID.SetOutputLimits(-1.0, 1.0);
  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd, P_ON_M);
  // make sure we start with a calculated PID output
  while (!myPID.Compute()) {}

  turn_off_target();
  lights.startup_animation(CURRENT_TEMPERATURE, 2000);
}

void loop(){

  turn_off_serial_lights();

#ifdef DEBUG_PLOTTER_ENABLED
  if (debug_plotter_timestamp + DEBUG_PLOTTER_INTERVAL < millis()) {
    debug_plotter_timestamp = millis();
    Serial.print(TARGET_TEMPERATURE);
    Serial.print('\t');
    Serial.print(CURRENT_TEMPERATURE);
    Serial.print('\t');
    Serial.println((TEMPERATURE_SWING * 50) + 50.0);
  }
#endif

  // update the peltiers' ON/OFF cycle
  peltiers.update_peltier_cycle();

  read_gcode();

  if (thermistor.update()) {
    CURRENT_TEMPERATURE = thermistor.plate_temperature();
  }

  // update the temperature display, and color-bar
  update_led_display(true);  // debounce enabled

  if (myPID.Compute()) {  // Compute() should run every loop
    if (MASTER_SET_A_TARGET) {
      stabilize_to_target_temp();
    }
    else {
      stabilize_to_room_temp();
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
