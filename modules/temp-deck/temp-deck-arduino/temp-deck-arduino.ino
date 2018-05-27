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
#define STABILIZING_ZONE 2

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

double TARGET_TEMPERATURE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE = TEMPERATURE_ROOM;
bool IS_TARGETING = false;

double TEMPERATURE_SWING;
double pid_Kp=25;  // reduces rise time, increases overshoot
double pid_Ki=0; // reduces steady-state error, increases overshoot
double pid_Kd=35;  // reduces overshoot, reduces settling time
PID myPID(&CURRENT_TEMPERATURE, &TEMPERATURE_SWING, &TARGET_TEMPERATURE, pid_Kp, pid_Ki, pid_Kd, DIRECT);

String device_serial = "TD001180622A01";
String device_model = "001";
String device_version = "edge-1a2b3c4";

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool stabilizing() {
  return abs(TARGET_TEMPERATURE - CURRENT_TEMPERATURE) < STABILIZING_ZONE;
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

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

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

  pid_Kd = 35;
  // lowering Kd makes it approach the target more aggressively
  // this is necessary for low temperatures, because they are harder to reach
  if (target_cold_zone()) {
    pid_Kd = 10;
  }
  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_percentage(float percentage){
  percentage = constrain(percentage, 0.0, 1.0);
  analogWrite(pin_fan_pwm, int(percentage * 255.0));
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void pid_stabilize() {
  if (myPID.Compute()) {
    if (TEMPERATURE_SWING > 0.0) {
      peltiers.set_hot_percentage(TEMPERATURE_SWING);
    }
    else {
      peltiers.set_cold_percentage(abs(TEMPERATURE_SWING));
    }
  }
}

void set_fan_from_temperature() {
  if (target_cold_zone()) {         // when target is cold, fan 100%
    set_fan_percentage(1.0);
  }
  else if(stabilizing()) {          // fan 50% when stabilizing in middle/hot range
    set_fan_percentage(0.5);
  }
  else if(TEMPERATURE_SWING < 0) {  // fan 100% when trying to get colder
    set_fan_percentage(1.0);
  }
  else {                            // default to 50%
    set_fan_percentage(0.5);
  }
}

void adjust_pid_from_temperature() {
  // default is full range
  myPID.SetOutputLimits(-1.0, 1.0);

  // don't let it cool down as much as it can while stabilizing
  if (stabilizing() && target_hot_zone()) {
    myPID.SetOutputLimits(-0.33, 1.0);
  }
  // don't let it heat up as much as it can while stabilizing
  else if (stabilizing() && target_cold_zone()) {
    myPID.SetOutputLimits(-1.0, 0.1);
  }
  // cold temperatures are hard to hold, so don't heat up much down there
  else if (target_cold_zone()) {
    myPID.SetOutputLimits(-1.0, 0.33);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void stabilize_to_target_temp(bool set_fan=true){
  unsigned long end_time = SET_TEMPERATURE_TIMESTAMP + millis_till_fan_turns_off;
  if (end_time > millis()) {
    peltiers.disable_peltiers();
    set_fan_percentage(0.0);
    return;  // exit this function, do not turn anything ON
  }
  end_time += millis_till_peltiers_drop_current;
  if (end_time > millis()) {
    set_fan = false;
  }
  if (set_fan == false) set_fan_percentage(0.0);
  else set_fan_from_temperature();
  adjust_pid_from_temperature();
  pid_stabilize();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void stabilize_to_room_temp() {
  if (burning_hot()) {
    adjust_pid_from_temperature();
    pid_stabilize();
    set_fan_percentage(0.5);
  }
  else {
    peltiers.disable_peltiers();
    set_fan_percentage(0.0);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_color_bar_from_temperature() {
  // if we are targetting, and close to the value, stop flashing
  if (!IS_TARGETING || abs(CURRENT_TEMPERATURE - TARGET_TEMPERATURE) < 1) {
    lights.flash_off();
  }
  // targetting and not close to value, continue flashing
  else {
    lights.flash_on();
  }

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

void update_temperature_display(boolean debounce=true){
  // round to closest whole-number temperature
  lights.display_number(CURRENT_TEMPERATURE + 0.5, debounce);
  set_color_bar_from_temperature();
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

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disengage() {
  set_target_temperature(TEMPERATURE_ROOM);
  IS_TARGETING = false;
  lights.flash_off();
}

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

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void start_dfu_timeout() {
  gcode.print_warning("Restarting and entering bootloader in 1 second...");
  START_BOOTLOADER = true;
  start_bootloader_timestamp = millis();
}

void check_if_bootloader_starts() {
  if (START_BOOTLOADER) {
    if (start_bootloader_timestamp + start_bootloader_timeout < millis()) {
      activate_bootloader();
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

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
          if (gcode.read_int('S')) {
            set_target_temperature(gcode.parsed_int);
            IS_TARGETING = true;
          }
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

void read_debug() {
  if (Serial.available() > 0) {
    if (Serial.peek() == 'k') {
      Serial.read();
      pid_Kp = Serial.parseFloat();
      pid_Ki = Serial.parseFloat();
      pid_Kd = Serial.parseFloat();
      myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd);
      Serial.print(" P=");Serial.println(pid_Kp);
      Serial.print(" I=");Serial.println(pid_Ki);
      Serial.print(" D=");Serial.println(pid_Kd);
    }
    else if (Serial.peek() == '\r') {
      Serial.println();
      Serial.print(" Target=");Serial.println(TARGET_TEMPERATURE);
      Serial.print(" Current=");Serial.println(CURRENT_TEMPERATURE);
      Serial.print(" Output=");Serial.println(TEMPERATURE_SWING);
      Serial.print(" P=");Serial.println(pid_Kp);
      Serial.print(" I=");Serial.println(pid_Ki);
      Serial.print(" D=");Serial.println(pid_Kd);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {
  gcode.setup(115200);

  set_fan_percentage(0.0);

  pinMode(pin_tone_out, OUTPUT);
  pinMode(pin_fan_pwm, OUTPUT);
  memory.read_serial(device_serial);
  memory.read_model(device_model);
  peltiers.setup_peltiers();
  lights.setup_lights();
  lights.set_numbers_brightness(0.5);
  lights.set_color_bar_brightness(1.0);

  // make sure we start with an averaged plate temperatures
  while (!thermistor.update()) {}
  CURRENT_TEMPERATURE = thermistor.plate_temperature();
  TARGET_TEMPERATURE = TEMPERATURE_ROOM;
  IS_TARGETING = false;

  // setup PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(100);
  myPID.SetOutputLimits(-1.0, 1.0);
  myPID.SetTunings(pid_Kp, pid_Ki, pid_Kd);

  disengage();
  update_temperature_display(false);  // force current temp to show (no debounce)
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop(){
  read_debug();
  read_gcode();

  if (thermistor.update()) {
    CURRENT_TEMPERATURE = thermistor.plate_temperature();
  }

  update_temperature_display(true);  // debounce enabled

  if (IS_TARGETING) {
    stabilize_to_target_temp();
  }
  else {
    stabilize_to_room_temp();
  }

  peltiers.update_peltier_cycle();

  check_if_bootloader_starts();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
