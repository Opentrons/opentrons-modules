#include "lid.h"

const char * Lid::LID_STATUS_STRINGS[] = { STATUS_TABLE };
volatile bool cover_switch_toggled = false;
volatile bool bottom_switch_toggled = false;
volatile unsigned long cover_switch_toggled_at = 0;
volatile unsigned long bottom_switch_toggled_at = 0;

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
  _update_status();
  return _status;
}

void Lid::_update_status()
{
  uint8_t status_bits = (_is_cover_switch_pressed << 1) | (_is_bottom_switch_pressed << 0);
  /* Truth Table */
  /* Bit 1 | Bit 0
  /*    0     0   -- Both switches CLOSED
   *    0     1   -- Bottom switch OPEN, cover switch CLOSED
   *    1     0   -- Bottom switch CLOSED, cover switch OPEN
   *    1     1   -- Both switches OPEN
   */
  switch(status_bits)
  {
    case 0x00:
      _status = Lid_status::in_between;
      break;
    case 0x01:
      _status = Lid_status::closed;
      break;
    case 0x02:
      _status = Lid_status::open;
      break;
    default:
      _status = Lid_status::error;
      break;
  }
}

void Lid::check_switches()
{
  if (cover_switch_toggled)
  {
    if (millis() - cover_switch_toggled_at >= 200)
    {
      cover_switch_toggled = false;
      _is_cover_switch_pressed = bool(digitalRead(PIN_COVER_SWITCH));
    }
  }
  if (bottom_switch_toggled)
  {
    if (millis() - bottom_switch_toggled_at >= 200)
    {
      bottom_switch_toggled = false;
      _is_bottom_switch_pressed = bool(digitalRead(PIN_BOTTOM_SWITCH));
    }
  }
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

bool Lid::move_millimeters(float mm)
{
  uint8_t dir = DIRECTION_UP;
  if (mm < 0) dir = DIRECTION_DOWN;
  unsigned long steps = abs(mm) * float(STEPS_PER_MM);
  motor_on();
  _reset_acceleration();
  for (unsigned long i=0;i<steps;i++)
  {
    _motor_step(dir);
    _update_acceleration();
    check_switches();
    if (dir)
    {
      if (_is_cover_switch_pressed)
      {
        return true;
      }
    }
    else
    {
      if (_is_bottom_switch_pressed)
      {
        return true;
      }
    }
  }
  motor_off();
  return false;
}

void Lid::open_cover()
{
  if (_is_cover_switch_pressed)
  {
     return;
  }
  move_millimeters(LID_MOTOR_RANGE_MM);
  motor_off();
}

void Lid::close_cover()
{
  if (_is_bottom_switch_pressed)
  {
    return;
  }
  move_millimeters(-LID_MOTOR_RANGE_MM);
  motor_off();
}

// Not a Lid class method
void _cover_switch_callback()
{
  cover_switch_toggled = true;
  cover_switch_toggled_at = millis();
}

// Not a Lid class method
void _bottom_switch_callback()
{
  bottom_switch_toggled = true;
  bottom_switch_toggled_at = millis();
}

void Lid::setup()
{
	pinMode(PIN_SOLENOID, OUTPUT);
	solenoid_off();
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_ENABLE, OUTPUT);
	motor_off();
#if DUMMY_BOARD
  pinMode(PIN_COVER_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BOTTOM_SWITCH, INPUT_PULLUP);
#else
  pinMode(PIN_COVER_SWITCH, INPUT);
  pinMode(PIN_BOTTOM_SWITCH, INPUT);
#endif
  _setup_digipot();
  delay(1);
  // Both switches are NORMALLY CLOSED
  _is_cover_switch_pressed = bool(digitalRead(PIN_COVER_SWITCH));
  _is_bottom_switch_pressed = bool(digitalRead(PIN_BOTTOM_SWITCH));
  _update_status();
  attachInterrupt(digitalPinToInterrupt(PIN_COVER_SWITCH), _cover_switch_callback, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BOTTOM_SWITCH), _bottom_switch_callback, CHANGE);
}
