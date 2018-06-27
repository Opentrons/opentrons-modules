//Device will:
////
//1) Home downwards to it's endstop
//2) Measure downwards to it's endstop
//3) Move to maximum-height with low current
//4) Move to found-height

#include <Arduino.h>
#include <avr/wdt.h>

#include <Wire.h>

// load in some custom classes for this device
#include "memory.h"
#include "gcodemagdeck.h"

String device_serial = "";  // leave empty, this value is read from eeprom during setup()
String device_model = "";   // leave empty, this value is read from eeprom during setup()
String device_version = "v1.0.0-beta1";

GcodeMagDeck gcode = GcodeMagDeck();  // reads in serial data to parse command and issue reponses
Memory memory = Memory();  // reads from EEPROM to find device's unique serial, and model number

#define ADDRESS_DIGIPOT 0x2D  // 7-bit address

#define MOTOR_ENGAGE_PIN 10
#define MOTOR_DIRECTION_PIN 9
#define MOTOR_STEP_PIN 6
#define LED_UP_PIN 5
#define LED_DOWN_PIN 13
#define ENDSTOP_PIN A5
#define ENDSTOP_PIN_TOP A4
#define TONE_PIN 11

#define DIRECTION_DOWN LOW
#define DIRECTION_UP HIGH

#define ENDSTOP_TRIGGERED_STATE LOW

#define CURRENT_TO_BYTES_FACTOR 77.0

#define STEPS_PER_MM 50  // full-stepping
unsigned long STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * 10);  // default 10mm/sec

#define MAX_TRAVEL_DISTANCE_MM 30
float FOUND_HEIGHT = MAX_TRAVEL_DISTANCE_MM - 5;

#define CURRENT_HIGH 0.3
#define CURRENT_LOW 0.075

#define HOMING_RETRACT 2

#define SPEED_HIGH 30
#define SPEED_LOW 7

float CURRENT_POSITION_MM = 0.0;
float SAVED_POSITION_OFFSET = 0;

float MM_PER_SEC = SPEED_LOW;

#define ACCELERATION_STARTING_DELAY_MICROSECONDS 1000
#define ACCELERATION_DELAY_FEEDBACK 0.9
#define PULSE_HIGH_MICROSECONDS 2

float ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS;

void i2c_write(byte address, byte value) {
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error) {
    Serial.print("Digipot I2C Error: "); Serial.println(error);
  }
}

void acceleration_reset() {
  ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS - STEP_DELAY_MICROSECONDS;
}

int get_next_acceleration_delay() {
  ACCELERATION_DELAY_MICROSECONDS -= ACCELERATION_DELAY_FEEDBACK;
  if(ACCELERATION_DELAY_MICROSECONDS <0) ACCELERATION_DELAY_MICROSECONDS = 0;
  return STEP_DELAY_MICROSECONDS + ACCELERATION_DELAY_MICROSECONDS;
}

void motor_step(uint8_t dir) {
  digitalWrite(LED_UP_PIN, dir);
  digitalWrite(LED_DOWN_PIN, 1 - dir);
  digitalWrite(MOTOR_DIRECTION_PIN, dir);
  digitalWrite(MOTOR_STEP_PIN, HIGH);
  delayMicroseconds(PULSE_HIGH_MICROSECONDS);
  digitalWrite(MOTOR_STEP_PIN, LOW);
  delayMicroseconds(get_next_acceleration_delay());  // this sets the speed!!
}


void set_speed(float mm_per_sec) {
//  Serial.print("\tSpeed: ");Serial.println(mm_per_sec);
  MM_PER_SEC = mm_per_sec;
  STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * MM_PER_SEC);
  STEP_DELAY_MICROSECONDS -= PULSE_HIGH_MICROSECONDS;
}

void set_current(float current) {
//  Serial.print("\tCurrent: ");Serial.println(current);
  // when wiper is set to "one_amp_byte_value", then current is 1.0 amp
  int c = CURRENT_TO_BYTES_FACTOR * current;
  if(c > 255) c = 255;
  // first wiper address on digi-pot is at location 0x00
  i2c_write(0x00, c);
}

float find_endstop(){
  unsigned long steps_taken = 0;
  acceleration_reset();
  enable_motor();
  while (digitalRead(ENDSTOP_PIN) != ENDSTOP_TRIGGERED_STATE) {
    motor_step(DIRECTION_DOWN);
    steps_taken++;
  }
  CURRENT_POSITION_MM = 0.0;
  float mm = steps_taken / STEPS_PER_MM;
  float remainder = steps_taken % STEPS_PER_MM;
  return mm + (remainder / float(STEPS_PER_MM));
}

void move_millimeters(float mm, boolean limit_switch=true){
//  Serial.print("MOVING "); Serial.print(mm); Serial.println("mm");
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) {
    dir = DIRECTION_DOWN;
  }
  unsigned long steps = abs(mm) * float(STEPS_PER_MM);
  acceleration_reset();
  boolean hit_endstop = false;
  enable_motor();
  for (unsigned long i=0;i<steps;i++) {
    motor_step(dir);
    if (limit_switch && digitalRead(ENDSTOP_PIN) == ENDSTOP_TRIGGERED_STATE) {
      hit_endstop = true;
      break;
    }
  }
  if (hit_endstop) {
    CURRENT_POSITION_MM = 0;
  }
  else {
    CURRENT_POSITION_MM += mm;
  }
  disable_motor();
}

float home_motor(bool save_distance=false) {
//  Serial.println("HOMING");
  set_current(CURRENT_HIGH);
  set_speed(SPEED_LOW);
  float f = find_endstop();
  move_millimeters(HOMING_RETRACT, false);
  disable_motor();
  return f;
}

void disable_motor() {
  digitalWrite(MOTOR_ENGAGE_PIN, HIGH);
}

void enable_motor() {
  digitalWrite(MOTOR_ENGAGE_PIN, LOW);
}

void low_current_rise(){
//  Serial.println("Low-Current Search...\n");
  set_current(CURRENT_LOW);
  set_speed(SPEED_LOW);
  move_millimeters(MAX_TRAVEL_DISTANCE_MM);
}

void move_to_position(float mm) {
  move_millimeters(mm - CURRENT_POSITION_MM);
}

void move_to_top(){
  set_current(CURRENT_HIGH);
  set_speed(SPEED_HIGH);
  move_to_position(FOUND_HEIGHT);
}

void move_to_bottom(){
  set_current(CURRENT_HIGH);
  set_speed(SPEED_HIGH);
  move_to_position(HOMING_RETRACT);
  home_motor();
}

void setup_digipot(){
  Wire.begin();
  Wire.setClock(20000);
  i2c_write(0x40, 0xff);
  i2c_write(0xA0, 0xff);
  set_current(CURRENT_LOW);
}

void setup_pins() {
  pinMode(LED_UP_PIN, OUTPUT);
  pinMode(LED_DOWN_PIN, OUTPUT);
  digitalWrite(LED_UP_PIN, HIGH);
  digitalWrite(LED_DOWN_PIN, HIGH);

  pinMode(MOTOR_ENGAGE_PIN, OUTPUT);
  pinMode(MOTOR_DIRECTION_PIN, OUTPUT);
  pinMode(MOTOR_STEP_PIN, OUTPUT);
  digitalWrite(MOTOR_ENGAGE_PIN, HIGH);  // begin disengaged
  digitalWrite(MOTOR_DIRECTION_PIN, LOW);
  digitalWrite(MOTOR_STEP_PIN, LOW);

  pinMode(ENDSTOP_PIN, INPUT);

  pinMode(TONE_PIN, OUTPUT);
  digitalWrite(TONE_PIN, LOW);
}

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

void setup() {
  setup_pins();
  setup_digipot();
  set_current(CURRENT_LOW);
  set_speed(SPEED_LOW);
  disable_motor();

  gcode.setup(115200);
  memory.read_serial(device_serial);
  memory.read_model(device_model);
}

void loop() {
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_HOME:
          home_motor();
          break;
        case GCODE_PROBE:
          low_current_rise();
          FOUND_HEIGHT = home_motor();
          break;
        case GCODE_GET_PROBED_DISTANCE:
          gcode.print_probed_distance(FOUND_HEIGHT);
          break;
        case GCODE_MOVE:
          if (gcode.read_number('Z')) {
            set_current(CURRENT_HIGH);
            set_speed(SPEED_HIGH);
            move_to_position(FOUND_HEIGHT - gcode.parsed_number);
          }
          break;
        case GCODE_GET_POSITION:
          gcode.print_current_position(CURRENT_POSITION_MM);
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
