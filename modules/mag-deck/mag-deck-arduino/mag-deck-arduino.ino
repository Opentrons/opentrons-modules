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
#include "magdeck.h"
#include "memory.h"
#include "motion.h"
#include "gcodemagdeck.h"

String device_serial = "";  // leave empty, this value is read from eeprom during setup()
String device_model = "";   // leave empty, this value is read from eeprom during setup()

unsigned int model_version;

GcodeMagDeck gcode = GcodeMagDeck();  // reads in serial data to parse command and issue reponses
Memory memory = Memory();  // reads from EEPROM to find device's unique serial, and model number
MotionParams motion = MotionParams();

void i2c_write(byte address, byte value) {
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error) {
    Serial.print("Digipot I2C Error: "); Serial.println(error);
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
  delayMicroseconds(motion.step_delay_microseconds % 1000);
  if (motion.step_delay_microseconds >= 1000) {
    delay(motion.step_delay_microseconds / 1000);
  }
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
  motion.acceleration_reset();
  enable_motor();
  while (digitalRead(ENDSTOP_PIN) != ENDSTOP_TRIGGERED_STATE) {
    motor_step(DIRECTION_DOWN, motion.get_next_acceleration_delay());
    steps_taken++;
  }
  motion.current_position_mm = 0.0;
  float mm = steps_taken / motion.steps_per_mm;
  float remainder = steps_taken % motion.steps_per_mm;
  return mm + (remainder / float(motion.steps_per_mm));
}

float home_motor(bool save_distance=false);

void move_millimeters(float mm, boolean limit_switch, float accel_factor=1.0){
//  Serial.print("MOVING "); Serial.print(mm); Serial.println("mm");
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) {
    dir = DIRECTION_DOWN;
  }
  unsigned long steps = abs(mm) * float(motion.steps_per_mm);
  motion.acceleration_reset(accel_factor);
  boolean hit_endstop = false;
  enable_motor();
  for (unsigned long i=0;i<steps;i++) {
    motion.enable_deceleration_if_needed(i, steps);
    motor_step(dir, motion.get_next_acceleration_delay());
    if (limit_switch && digitalRead(ENDSTOP_PIN) == ENDSTOP_TRIGGERED_STATE) {
      hit_endstop = true;
      break;
    }
  }
  if (hit_endstop) {
    home_motor();
  }
  else {
    motion.current_position_mm += mm;
    disable_motor();
  }
}

float home_motor(bool save_distance=false) {
//  Serial.println("HOMING");
  set_lights_down();
  set_current(CURRENT_HIGH);
  motion.set_speed(motion.speed_low);
  float f = find_endstop();
  move_millimeters(HOMING_RETRACT, false);
  motion.current_position_mm = 0;
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
  if (mm < motion.current_position_mm) {
    set_lights_down();
  }
  else {
    set_lights_up();
  }
  move_millimeters(mm - motion.current_position_mm, limit_switch, accel_factory);
  if (int(motion.current_position_mm) < 1) {
    set_lights_down();
  }
  else {
    set_lights_up();
  }
}

void move_to_top(){
  set_current(CURRENT_HIGH);
  motion.set_speed(motion.speed_high);
  move_to_position(motion.found_height);
}

void move_to_bottom(){
  set_current(CURRENT_HIGH);
  motion.set_speed(motion.speed_high);
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
  memory.read_serial(device_serial);
  memory.read_model(device_model);

  model_version = device_model.substring(MODEL_VER_TEMPLATE_LEN).toInt();
  motion = MotionParams(model_version);

  setup_pins();
  setup_digipot();
  set_current(CURRENT_HIGH);
  motion.set_speed(motion.speed_low);
  disable_motor();

  gcode.setup(115200);

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
          if (gcode.read_number('F')) motion.set_speed(gcode.parsed_number);
          else motion.set_speed(motion.speed_probe);
          move_to_position(MAX_TRAVEL_DISTANCE_MM, false, 2.0);  // 2x slower acceleration
          motion.found_height = home_motor();
          break;
        case GCODE_GET_PROBED_DISTANCE:
          gcode.print_probed_distance(motion.found_height);
          break;
        case GCODE_MOVE:
          if (gcode.read_number('C')) set_current(gcode.parsed_number);
          else set_current(CURRENT_HIGH);
          if (gcode.read_number('F')) motion.set_speed(gcode.parsed_number);
          else motion.set_speed(motion.speed_high);
          if (gcode.read_number('Z')) {
            move_to_position(gcode.parsed_number);
          }
          break;
        case GCODE_GET_POSITION:
          gcode.print_current_position(motion.current_position_mm);
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, FW_VERSION);
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
