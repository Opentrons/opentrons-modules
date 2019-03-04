/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include "lid.h"

Lid::Lid() {}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void Lid::i2c_write(byte address, byte value) {
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error) {
    Serial.print("Digipot I2C Error: "); Serial.println(error);
  }
}

void Lid::set_current(float current) {
  // Serial.print("\tCurrent: ");Serial.println(current);
  // when wiper is set to "one_amp_byte_value", then current is 1.0 amp
  int c = 255.0 * current * 0.5;
  if(c > 255) c = 255;
  // first wiper address on digi-pot is at location 0x00
  i2c_write(AD5110_SET_VALUE_CMD, c);
  delay(SET_CURRENT_DELAY_MS);
}

void Lid::save_current() {
  i2c_write(AD5110_SAVE_VALUE_CMD, 0x00);
  delay(SET_CURRENT_DELAY_MS);
}

void Lid::setup_digipot(){
  Wire.begin();
  // make sure that on power-up, the digi-pot sets the current to 0.0amps
  set_current(0.0);
  save_current();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
Lid_status Lid::status() {
  if(is_open_switch_pressed() && !is_closed_switch_pressed()) {
    return OPEN;
  }
  else if(is_closed_switch_pressed() && !is_open_switch_pressed()) {
    return CLOSED;
  }
  else {
    return UNKNOWN;
  }
}

bool Lid::is_open_switch_pressed() {
  return (digitalRead(PIN_COVER_OPEN_SWITCH) == SWITCH_PRESSED_STATE);
}

bool Lid::is_closed_switch_pressed() {
  return (digitalRead(PIN_COVER_CLOSED_SWITCH) == SWITCH_PRESSED_STATE);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void Lid::solenoid_on() {
  digitalWrite(PIN_SOLENOID, SOLENOID_STATE_ON);
  // don't do anything until sure it's open
  delay(SOLENOID_TIME_TO_OPEN_MILLISECONDS);
}

void Lid::solenoid_off() {
  digitalWrite(PIN_SOLENOID, SOLENOID_STATE_OFF);
  // don't do anything until sure it's closed
  delay(SOLENOID_TIME_TO_OPEN_MILLISECONDS);
}

void Lid::motor_off() {
  digitalWrite(PIN_STEPPER_ENABLE, STEPPER_OFF_STATE);
}

void Lid::motor_on() {
  digitalWrite(PIN_STEPPER_ENABLE, STEPPER_ON_STATE);
  delay(MOTOR_ENABLE_DELAY_MS);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void Lid::motor_step(uint8_t dir) {
  digitalWrite(PIN_STEPPER_DIR, dir);
  digitalWrite(PIN_STEPPER_STEP, HIGH);
  delayMicroseconds(PULSE_HIGH_MICROSECONDS);
  digitalWrite(PIN_STEPPER_STEP, LOW);
  delayMicroseconds(long(STEP_DELAY_MICROSECONDS) % 1000);
  if (STEP_DELAY_MICROSECONDS >= 1000) {
    delay(STEP_DELAY_MICROSECONDS / 1000);
  }
}

void Lid::set_speed(float mm_per_sec) {
//  Serial.print("\tSpeed: ");Serial.println(mm_per_sec);
  MM_PER_SEC = mm_per_sec;
}

void Lid::set_acceleration(float mm_per_sec_per_sec) {
  ACCELERATION = mm_per_sec_per_sec;
}

void Lid::reset_acceleration(){
  _active_mm_per_sec = _start_mm_per_sec;
  calculate_step_delay();
}

void Lid::calculate_step_delay() {
  STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * _active_mm_per_sec);
  STEP_DELAY_MICROSECONDS -= PULSE_HIGH_MICROSECONDS;
}

void Lid::update_acceleration() {
  // take the time since the last step, and calculate how much to adjust mm/sec
  _active_mm_per_sec += ((STEP_DELAY_MICROSECONDS + PULSE_HIGH_MICROSECONDS) / 1000000) * ACCELERATION;
  _active_mm_per_sec = min(_active_mm_per_sec, MM_PER_SEC);
  calculate_step_delay();
}

void Lid::move_millimeters(float mm, bool top_switch, bool bottom_switch){
  // Serial.print("MOVING "); Serial.print(mm); Serial.println("mm");
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) dir = DIRECTION_DOWN;
  unsigned long steps = abs(mm) * float(STEPS_PER_MM);
  motor_on();
  reset_acceleration();
  for (unsigned long i=0;i<steps;i++) {
    motor_step(dir);
    update_acceleration();
    if (top_switch && is_open_switch_pressed()) return;
    if (bottom_switch && is_closed_switch_pressed()) return;
  }
  motor_off();
}

void Lid::open_cover() {
  if (is_open_switch_pressed()) {
     return;
  }
  move_millimeters(300, true, false);
  motor_off();
}

void Lid::close_cover() {
  if (is_closed_switch_pressed()) {
    return;
  }
  move_millimeters(-300, false, true);
  motor_off();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void Lid::setup() {
	pinMode(PIN_SOLENOID, OUTPUT);
	solenoid_off();
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_ENABLE, OUTPUT);
	motor_off();
	pinMode(PIN_COVER_OPEN_SWITCH, INPUT);
	pinMode(PIN_COVER_CLOSED_SWITCH, INPUT);
	setup_digipot();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
