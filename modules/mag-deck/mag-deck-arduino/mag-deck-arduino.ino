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
String device_version = "v1.0.1";

GcodeMagDeck gcode = GcodeMagDeck();  // reads in serial data to parse command and issue reponses
Memory memory = Memory();  // reads from EEPROM to find device's unique serial, and model number

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

#define STEPS_PER_MM 50  // full-stepping
unsigned long STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * 10);  // default 10mm/sec

#define MAX_TRAVEL_DISTANCE_MM 40
float FOUND_HEIGHT = MAX_TRAVEL_DISTANCE_MM - 15;

#define CURRENT_HIGH 0.625
#define CURRENT_LOW 0.02
#define SET_CURRENT_DELAY_MS 20
#define ENABLE_DELAY_MS 20

#define HOMING_RETRACT 2

#define SPEED_HIGH 50
#define SPEED_LOW 20
#define SPEED_PROBE 10

float CURRENT_POSITION_MM = 0.0;
float SAVED_POSITION_OFFSET = 0.0;

float MM_PER_SEC = SPEED_LOW;

#define ACCELERATION_STARTING_DELAY_MICROSECONDS 1000
#define DEFAULT_ACCELERATION_DELAY_FEEDBACK 0.9  // bigger number means faster acceleration
#define PULSE_HIGH_MICROSECONDS 2

float ACCELERATION_DELAY_FEEDBACK = DEFAULT_ACCELERATION_DELAY_FEEDBACK;
float ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS;
const int steps_per_acceleration_cycle = ACCELERATION_STARTING_DELAY_MICROSECONDS / ACCELERATION_DELAY_FEEDBACK;

void i2c_write(byte address, byte value) {
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error) {
    Serial.print("Digipot I2C Error: "); Serial.println(error);
  }
}

#define ACCELERATE_OFF 0
#define ACCELERATE_DOWN 1
#define ACCELERATE_UP 2
uint8_t accelerate_direction = ACCELERATE_OFF;
float acceleration_factor = 1.0;
unsigned long number_of_acceleration_steps = 0;

void acceleration_reset(float factor=1.0) {
  acceleration_factor = factor;
  ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS - STEP_DELAY_MICROSECONDS;
  ACCELERATION_DELAY_FEEDBACK = DEFAULT_ACCELERATION_DELAY_FEEDBACK / factor;
  accelerate_direction = ACCELERATE_UP;
  number_of_acceleration_steps = 0;
}

int get_next_acceleration_delay() {
  if (accelerate_direction == ACCELERATE_UP) {
    ACCELERATION_DELAY_MICROSECONDS -= ACCELERATION_DELAY_FEEDBACK;
    number_of_acceleration_steps += 1;
    if(ACCELERATION_DELAY_MICROSECONDS < 0) {
      ACCELERATION_DELAY_MICROSECONDS = 0;
      accelerate_direction = ACCELERATE_OFF;
    }
  }
  else if (accelerate_direction == ACCELERATE_DOWN){
    ACCELERATION_DELAY_MICROSECONDS += ACCELERATION_DELAY_FEEDBACK;
    if(ACCELERATION_DELAY_MICROSECONDS > ACCELERATION_STARTING_DELAY_MICROSECONDS - STEP_DELAY_MICROSECONDS) {
      ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS - STEP_DELAY_MICROSECONDS;
      accelerate_direction = ACCELERATE_OFF;
    }
  }
  return ACCELERATION_DELAY_MICROSECONDS;
}

void enable_deceleration_if_needed(int current_step, int total_steps) {
  if (total_steps - current_step <= number_of_acceleration_steps) {
    accelerate_direction = ACCELERATE_DOWN;
  }
}

void set_lights_up() {
  digitalWrite(LED_UP_PIN, HIGH);
  digitalWrite(LED_DOWN_PIN, LOW);
}

void set_lights_down() {
  digitalWrite(LED_UP_PIN, LOW);
  digitalWrite(LED_DOWN_PIN, HIGH);
}

void motor_step(uint8_t dir, int speed_delay) {
  digitalWrite(MOTOR_DIRECTION_PIN, dir);
  digitalWrite(MOTOR_STEP_PIN, HIGH);
  delayMicroseconds(PULSE_HIGH_MICROSECONDS);
  digitalWrite(MOTOR_STEP_PIN, LOW);
  delayMicroseconds(speed_delay);  // this sets the speed!!
  delayMicroseconds(STEP_DELAY_MICROSECONDS % 1000);
  if (STEP_DELAY_MICROSECONDS >= 1000) {
    delay(STEP_DELAY_MICROSECONDS / 1000);
  }
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
  delay(SET_CURRENT_DELAY_MS);
}

float find_endstop(){
  unsigned long steps_taken = 0;
  acceleration_reset();
  enable_motor();
  while (digitalRead(ENDSTOP_PIN) != ENDSTOP_TRIGGERED_STATE) {
    motor_step(DIRECTION_DOWN, get_next_acceleration_delay());
    steps_taken++;
  }
  CURRENT_POSITION_MM = 0.0;
  float mm = steps_taken / STEPS_PER_MM;
  float remainder = steps_taken % STEPS_PER_MM;
  return mm + (remainder / float(STEPS_PER_MM));
}

float home_motor(bool save_distance=false);

void move_millimeters(float mm, boolean limit_switch, float accel_factor=1.0){
//  Serial.print("MOVING "); Serial.print(mm); Serial.println("mm");
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) {
    dir = DIRECTION_DOWN;
  }
  unsigned long steps = abs(mm) * float(STEPS_PER_MM);
  acceleration_reset(accel_factor);
  boolean hit_endstop = false;
  enable_motor();
  for (unsigned long i=0;i<steps;i++) {
    enable_deceleration_if_needed(i, steps);
    motor_step(dir, get_next_acceleration_delay());
    if (limit_switch && digitalRead(ENDSTOP_PIN) == ENDSTOP_TRIGGERED_STATE) {
      hit_endstop = true;
      break;
    }
  }
  if (hit_endstop) {
    home_motor();
  }
  else {
    CURRENT_POSITION_MM += mm;
    disable_motor();
  }
}

float home_motor(bool save_distance=false) {
//  Serial.println("HOMING");
  set_lights_down();
  set_current(CURRENT_HIGH);
  set_speed(SPEED_LOW);
  float f = find_endstop();
  move_millimeters(HOMING_RETRACT, false);
  CURRENT_POSITION_MM = 0;
  disable_motor();
  set_lights_down();
  return f - HOMING_RETRACT;
}

void disable_motor() {
  digitalWrite(MOTOR_ENGAGE_PIN, HIGH);
}

void enable_motor() {
  digitalWrite(MOTOR_ENGAGE_PIN, LOW);
  delay(ENABLE_DELAY_MS);
}

void move_to_position(float mm, bool limit_switch=true, float accel_factory=1.0) {
  if (mm < CURRENT_POSITION_MM) {
    set_lights_down();
  }
  else {
    set_lights_up();
  }
  move_millimeters(mm - CURRENT_POSITION_MM, limit_switch, accel_factory);
  if (int(CURRENT_POSITION_MM) < 1) {
    set_lights_down();
  }
  else {
    set_lights_up();
  }
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
  unsigned long tempWait = millis();
  while(millis() - tempWait < 3000){
    // Pi has 3 seconds to cleanly disconnect
  }
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
  set_current(CURRENT_HIGH);
  set_speed(SPEED_LOW);
  disable_motor();

  gcode.setup(115200);
  memory.read_serial(device_serial);
  memory.read_model(device_model);

  delay(1000);
  home_motor();
  delay(1000);

  tone(TONE_PIN, 440, 200);
  tone(TONE_PIN, 440 * 2, 200);
  tone(TONE_PIN, 440 * 2 * 2, 200);

}

void loop() {
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_HOME:
          home_motor();
          break;
        case GCODE_PROBE:
          if (gcode.read_number('C')) set_current(gcode.parsed_number);
          else set_current(CURRENT_LOW);
          if (gcode.read_number('F')) set_speed(gcode.parsed_number);
          else set_speed(SPEED_PROBE);
          move_to_position(MAX_TRAVEL_DISTANCE_MM, false, 2.0);  // 2x slower acceleration
          FOUND_HEIGHT = home_motor();
          break;
        case GCODE_GET_PROBED_DISTANCE:
          gcode.print_probed_distance(FOUND_HEIGHT);
          break;
        case GCODE_MOVE:
          if (gcode.read_number('C')) set_current(gcode.parsed_number);
          else set_current(CURRENT_HIGH);
          if (gcode.read_number('F')) set_speed(gcode.parsed_number);
          else set_speed(SPEED_HIGH);
          if (gcode.read_number('Z')) {
            move_to_position(gcode.parsed_number);
          }
          break;
        case GCODE_GET_POSITION:
          gcode.print_current_position(CURRENT_POSITION_MM);
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, device_version);
          break;
        case GCODE_DFU:
          gcode.send_ack(); // Send ack here since we not reaching the end of the loop
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
