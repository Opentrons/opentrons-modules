/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Arduino.h>
#include "memory.h"
#include "lights.h"
#include "peltiers.h"
#include "thermistor.h"
#include "gcode.h"

#include <PID_v1.h>

#define pin_tone_out 11
#define pin_fan_pwm 9

#define TEMPERATURE_MAX 99
#define TEMPERATURE_MIN -9

#define TEMPERATURE_FAN_CUTOFF_COLD 15
#define TEMPERATURE_FAN_CUTOFF_HOT 35
#define TEMPERATURE_ROOM 25
#define TEMPERATURE_BURN 55
#define STABILIZING_ZONE 1

#define FAN_HIGH 0.85
#define FAN_LOW 0.4
#define FAN_OFF 0.0

#define DEFAULT_PID_KD 0.0

#define DOWN_PID_KP 0.38
#define DOWN_PID_KI 0.0275

#define UP_MIN_PID_KP 0.17
#define UP_MIN_PID_KI 0.012

#define UP_MAX_PID_KP 0.26
#define UP_MAX_PID_KI 0.0225

#define UP_MIN_PID_TEMP 40
#define UP_MAX_PID_TEMP 100

#define PID_ENABLED_ZONE 10

// uncomment below 3 lines to print temperature and PID information
// to use with the Arduino IDE's "Serial Plotter" graphing tool

// #define DEBUG_PLOTTER_ENABLED 1
// #define DEBUG_PLOTTER_INTERVAL 250
// unsigned long debug_plotter_timestamp = 0;

Lights lights = Lights();
Peltiers peltiers = Peltiers();
Thermistor thermistor = Thermistor();
Gcode gcode = Gcode();
Memory memory = Memory();

bool START_BOOTLOADER = false;
unsigned long start_bootloader_timestamp = 0;
const int start_bootloader_timeout = 1000;

unsigned long SET_TEMPERATURE_TIMESTAMP = 0;
const unsigned long millis_till_fan_turns_off = 200;
const unsigned long millis_till_peltiers_drop_current = 200;
unsigned long now;

double TARGET_TEMPERATURE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE = TEMPERATURE_ROOM;
bool IS_TARGETING = false;

double TEMPERATURE_SWING;  // -1.0 is full cold peltiers, +1.0 is full hot peltiers, can be between
double pid_Kp = DOWN_PID_KP;  // reduces rise time, increases overshoot
double pid_Ki = DOWN_PID_KI;  // reduces steady-state error, increases overshoot
double pid_Kd = DEFAULT_PID_KD;
PID myPID(&CURRENT_TEMPERATURE, &TEMPERATURE_SWING, &TARGET_TEMPERATURE, pid_Kp, pid_Ki, pid_Kd, P_ON_M, DIRECT);

String device_serial = "";  // leave empty, this value is read from eeprom during setup()
String device_model = "";   // leave empty, this value is read from eeprom during setup()
String device_version = "edge-1a2b3c4";

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

void set_target_temperature(int target_temp){
  if (target_temp < TEMPERATURE_MIN) {
    target_temp = TEMPERATURE_MIN;
    gcode.print_warning(
      "Target temperature too low, setting to TEMPERATURE_MIN degrees");
  }
  if (target_temp > TEMPERATURE_MAX) {
    target_temp = TEMPERATURE_MAX;
    gcode.print_warning(
      "Target temperature too high, setting to TEMPERATURE_MAX degrees");
  }
  TARGET_TEMPERATURE = target_temp;
  SET_TEMPERATURE_TIMESTAMP = millis();
  lights.flash_on();

  adjust_pid_on_new_target();
}

void disengage() {
  set_target_temperature(TEMPERATURE_ROOM);
  IS_TARGETING = false;
  lights.flash_off();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_power(float percentage){
  percentage = constrain(percentage, 0.0, 1.0);
  analogWrite(pin_fan_pwm, int(percentage * 255.0));
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
    if (TARGET_TEMPERATURE <= UP_MIN_PID_TEMP) {
      pid_Kp = UP_MIN_PID_KP;
      pid_Ki = UP_MIN_PID_KI;
    }
    else if (TARGET_TEMPERATURE >= UP_MAX_PID_TEMP) {
      pid_Kp = UP_MAX_PID_KP;
      pid_Ki = UP_MAX_PID_KI;
    }
    else {
      // linear function between the MIN and MAX Kp and Ki values when moving up
      float scaler = (TARGET_TEMPERATURE - UP_MIN_PID_TEMP) / (UP_MAX_PID_TEMP - UP_MIN_PID_TEMP);
      pid_Kp = UP_MIN_PID_KP + (scaler * (UP_MAX_PID_KP - UP_MIN_PID_KP));
      pid_Ki = UP_MIN_PID_KI + (scaler * (UP_MAX_PID_KI - UP_MIN_PID_KI));
    }
  }

  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd, P_ON_M);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void stabilize_to_target_temp(bool set_fan=true){
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

void stabilize_to_room_temp() {
  if (burning_hot()) {
    set_peltiers_from_pid();
    set_fan_power(FAN_LOW);
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
  if (!IS_TARGETING || stabilizing()) {
    lights.flash_off();
  }
  // targetting and not close to value, continue flashing
  else {
    lights.flash_on();
  }

  // set the color-bar color depending on if we're targing hot or cold temperatures
  if (!IS_TARGETING) {
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
  // Need to verify if a reset to application code
  // is performed properly after DFU
  cli();
  // disable watchdog, if enabled
  // disable all peripherals
  UDCON = 1;
  USBCON = (1<<FRZCLK);  // disable USB
  UCSR1B = 0;
  _delay_ms(5);
  #if defined(__AVR_ATmega32U4__)
      EIMSK = 0; PCICR = 0; SPCR = 0; ACSR = 0; EECR = 0; ADCSRA = 0;
      TIMSK0 = 0; TIMSK1 = 0; TIMSK3 = 0; TIMSK4 = 0; UCSR1B = 0; TWCR = 0;
      DDRB = 0; DDRC = 0; DDRD = 0; DDRE = 0; DDRF = 0; TWCR = 0;
      PORTB = 0; PORTC = 0; PORTD = 0; PORTE = 0; PORTF = 0;
      asm volatile("jmp 0x3800");   //Bootloader start address
  #endif
}

void start_dfu_timeout() {
  gcode.print_warning("Restarting and entering bootloader in 1 second...");
  START_BOOTLOADER = true;
  start_bootloader_timestamp = millis();
}

void check_if_bootloader_starts() {
  now = millis();
  if (start_bootloader_timestamp > now) start_bootloader_timestamp = now;  // handle rollover
  if (START_BOOTLOADER) {
    if (start_bootloader_timestamp + start_bootloader_timeout < now) {
      activate_bootloader();
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void print_temperature() {
  if (IS_TARGETING) {
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
            IS_TARGETING = true;
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
          disengage();
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, device_version);
          break;
        case GCODE_DFU:
          start_dfu_timeout();
          break;
        default:
          break;
      }
    }
    gcode.send_ack();
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

  gcode.setup(115200);

  set_fan_power(FAN_OFF);

  pinMode(pin_tone_out, OUTPUT);
  pinMode(pin_fan_pwm, OUTPUT);
  memory.read_serial(device_serial);
  memory.read_model(device_model);
  peltiers.setup_peltiers();
  lights.setup_lights();
  lights.set_numbers_brightness(0.25);
  lights.set_color_bar_brightness(0.5);

  // make sure we start with an averaged plate temperatures
  while (!thermistor.update()) {}
  CURRENT_TEMPERATURE = thermistor.plate_temperature();
  TARGET_TEMPERATURE = TEMPERATURE_ROOM;

  // setup PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(500);
  myPID.SetOutputLimits(-1.0, 1.0);
  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd, P_ON_M);

  disengage();
  lights.startup_animation(CURRENT_TEMPERATURE, 2000);
}

void loop(){

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

  read_gcode();

  if (thermistor.update()) {
    CURRENT_TEMPERATURE = thermistor.plate_temperature();
  }

  // update the temperature display, and color-bar
  update_led_display(true);  // debounce enabled

  if (myPID.Compute() && IS_TARGETING) {  // Compute() should run every loop
    stabilize_to_target_temp();
  }
  else {
    stabilize_to_room_temp();
  }

  // update the peltiers' ON/OFF cycle
  peltiers.update_peltier_cycle();

  check_if_bootloader_starts();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
