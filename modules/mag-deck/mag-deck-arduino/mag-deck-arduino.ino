//Device will:
////
//1) Home downwards to it's endstop
//2) Measure downwards to it's endstop
//3) Move to maximum-height with low current
//4) Move to found-height

#include <Arduino.h>
#include <Wire.h>

const char ADDRESS_DIGIPOT = 0x2D;  // 7-bit address

const uint8_t MOTOR_ENGAGE_PIN = 10;
const uint8_t MOTOR_DIRECTION_PIN = 9;
const uint8_t MOTOR_STEP_PIN = 6;
const uint8_t LED_UP_PIN = 5;
const uint8_t LED_DOWN_PIN = 13;
const uint8_t ENDSTOP_PIN = A5;
const uint8_t TONE_PIN = 11;

const uint8_t DIRECTION_DOWN = HIGH;
const uint8_t DIRECTION_UP = LOW;

const uint8_t ENDSTOP_TRIGGERED_STATE = LOW;

const float CURRENT_TO_BYTES_FACTOR = 77.0;

const unsigned long STEPS_PER_MM = 50;  // full-stepping
unsigned long STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * 10);  // default 10mm/sec

const float MAX_TRAVEL_DISTANCE_MM = 30;
float FOUND_HEIGHT = MAX_TRAVEL_DISTANCE_MM - 5;

const float CURRENT_HIGH = 0.3;
const float CURRENT_LOW = 0.075;

float CURRENT_POSITION_MM = 0.0;
const int HOMING_RETRACT = 2;

const float SPEED_HIGH = 30;
const float SPEED_LOW = 7;

float SAVED_POSITION_OFFSET = 0;

float MM_PER_SEC = SPEED_LOW;

const float ACCELERATION_STARTING_DELAY_MICROSECONDS = 1000;
const float ACCELERATION_DELAY_FEEDBACK = 0.9;
float ACCELERATION_DELAY_MICROSECONDS = ACCELERATION_STARTING_DELAY_MICROSECONDS;
const uint8_t PULSE_HIGH_MICROSECONDS = 2;

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

void serial_ack(){
  Serial.println("ok");
  Serial.println("ok");
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(30);
  setup_pins();
  setup_digipot();
  set_current(CURRENT_LOW);
  set_speed(SPEED_LOW);
  disable_motor();
}

void loop() {
  if (Serial.available()){
    char l = Serial.read();
    if (l == 'h') {
      home_motor();
      serial_ack();
    }
    if (l == 't') {
      home_motor();
      serial_ack();
    }
    else if (l == 'm') {
      low_current_rise();
      FOUND_HEIGHT = home_motor();
      serial_ack();
    }
    else if (l == '=') {
      move_to_top();
      serial_ack();
    }
    else if (l == '-') {
      move_to_bottom();
      serial_ack();
    }
    else if(l == 'w') {
      set_current(CURRENT_HIGH);
      set_speed(SPEED_HIGH);
      move_to_position(FOUND_HEIGHT - 0.35);
      serial_ack();
    }
    else if (l == 'j') {
      set_current(CURRENT_HIGH);
      set_speed(SPEED_HIGH);
      move_millimeters(-1);
    }
    else if (l == 'p') {
      set_current(CURRENT_HIGH);
      set_speed(SPEED_HIGH);
      move_to_position(FOUND_HEIGHT - Serial.parseFloat());
      serial_ack();
    }
  }
}
