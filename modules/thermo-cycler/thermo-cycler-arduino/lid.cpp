#include "lid.h"

const char * Lid::LID_STATUS_STRINGS[] = { STATUS_TABLE };

Lid::Lid()
{}

void Lid::_i2c_write(byte address, byte value)
{
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error)
  {
    Serial.print("Digipot I2C Error: "); Serial.println(error);
  }
}

void Lid::set_current(float current)
{
  // when wiper is set to "one_amp_byte_value", then current is 1.0 amp
  int c = 255.0 * current * 0.5;
  if(c > 255)
  {
    c = 255;
  }
  // first wiper address on digi-pot is at location 0x00
  _i2c_write(AD5110_SET_VALUE_CMD, c);
  delay(SET_CURRENT_DELAY_MS);
}

void Lid::_save_current()
{
  _i2c_write(AD5110_SAVE_VALUE_CMD, 0x00);
  delay(SET_CURRENT_DELAY_MS);
}

void Lid::_setup_digipot()
{
  Wire.begin();
  // make sure that on power-up, the digi-pot sets the current to 0.0amps
  set_current(0.0);
  _save_current();
}

Lid_status Lid::status()
{
  if(_is_open_switch_pressed() && !_is_closed_switch_pressed())
  {
    return Lid_status::open;
  }
  else if(_is_closed_switch_pressed() && !_is_open_switch_pressed())
  {
    return Lid_status::closed;
  }
  else
  {
    return Lid_status::unknown;
  }
}

bool Lid::_is_open_switch_pressed()
 {
  return (digitalRead(PIN_COVER_OPEN_SWITCH) == SWITCH_PRESSED_STATE);
}

bool Lid::_is_closed_switch_pressed()
{
  return (digitalRead(PIN_COVER_CLOSED_SWITCH) == SWITCH_PRESSED_STATE);
}

void Lid::solenoid_on()
{
  digitalWrite(PIN_SOLENOID, SOLENOID_STATE_ON);
  // don't do anything until sure it's open
  delay(SOLENOID_TIME_TO_OPEN_MILLISECONDS);
}

void Lid::solenoid_off()
{
  digitalWrite(PIN_SOLENOID, SOLENOID_STATE_OFF);
  // don't do anything until sure it's closed
  delay(SOLENOID_TIME_TO_OPEN_MILLISECONDS);
}

void Lid::motor_off()
{
  digitalWrite(PIN_STEPPER_ENABLE, STEPPER_OFF_STATE);
}

void Lid::motor_on()
{
  digitalWrite(PIN_STEPPER_ENABLE, STEPPER_ON_STATE);
  delay(MOTOR_ENABLE_DELAY_MS);
}

void Lid::_motor_step(uint8_t dir)
{
  digitalWrite(PIN_STEPPER_DIR, dir);
  digitalWrite(PIN_STEPPER_STEP, HIGH);
  delayMicroseconds(PULSE_HIGH_MICROSECONDS);
  digitalWrite(PIN_STEPPER_STEP, LOW);
  delayMicroseconds(long(_step_delay_microseconds) % 1000);
  if (_step_delay_microseconds >= 1000)
  {
    delay(_step_delay_microseconds / 1000);
  }
}

void Lid::set_speed(float mm_per_sec)
{
//  Serial.print("\tSpeed: ");Serial.println(mm_per_sec);
  _mm_per_sec = mm_per_sec;
}

void Lid::set_acceleration(float mm_per_sec_per_sec)
{
  _acceleration = mm_per_sec_per_sec;
}

void Lid::_reset_acceleration()
{
  _active_mm_per_sec = _start_mm_per_sec;
  _calculate_step_delay();
}

void Lid::_calculate_step_delay()
{
  _step_delay_microseconds = 1000000 / (STEPS_PER_MM * _active_mm_per_sec);
  _step_delay_microseconds -= PULSE_HIGH_MICROSECONDS;
}

void Lid::_update_acceleration()
{
  // take the time since the last step, and calculate how much to adjust mm/sec
  _active_mm_per_sec += ((_step_delay_microseconds + PULSE_HIGH_MICROSECONDS) / 1000000) * _acceleration;
  _active_mm_per_sec = min(_active_mm_per_sec, _mm_per_sec);
  _calculate_step_delay();
}

void Lid::move_millimeters(float mm, bool top_switch, bool bottom_switch)
{
  // Serial.print("MOVING "); Serial.print(mm); Serial.println("mm");
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) dir = DIRECTION_DOWN;
  unsigned long steps = abs(mm) * float(STEPS_PER_MM);
  motor_on();
  _reset_acceleration();
  for (unsigned long i=0;i<steps;i++)
  {
    _motor_step(dir);
    _update_acceleration();
    if (top_switch && _is_open_switch_pressed()) return;
    if (bottom_switch && _is_closed_switch_pressed()) return;
  }
  motor_off();
}

void Lid::open_cover()
{
  if (_is_open_switch_pressed())
  {
     return;
  }
  move_millimeters(300, true, false);
  motor_off();
}

void Lid::close_cover()
{
  if (_is_closed_switch_pressed())
  {
    return;
  }
  move_millimeters(-300, false, true);
  motor_off();
}

void Lid::setup()
{
	pinMode(PIN_SOLENOID, OUTPUT);
	solenoid_off();
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_ENABLE, OUTPUT);
	motor_off();
	pinMode(PIN_COVER_OPEN_SWITCH, INPUT);
	pinMode(PIN_COVER_CLOSED_SWITCH, INPUT);
	_setup_digipot();
}
