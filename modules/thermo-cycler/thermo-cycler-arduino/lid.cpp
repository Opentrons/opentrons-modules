#include "lid.h"

const char * Lid::LID_STATUS_STRINGS[] = { STATUS_TABLE };
volatile bool cover_switch_toggled = false;
volatile bool bottom_switch_toggled = false;
volatile bool motor_driver_faulted = false;
volatile unsigned long cover_switch_toggled_at = 0;
volatile unsigned long bottom_switch_toggled_at = 0;

Lid::Lid()
{}

bool Lid::_i2c_write(byte address, byte value)
{
  Wire.beginTransmission(ADDRESS_DIGIPOT);
  Wire.write(address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error)
  {
    return false;
  }
  return true;
}

byte Lid::_i2c_read()
{
  Wire.requestFrom(ADDRESS_DIGIPOT, 1); // request 1 byte from the digipot
  delay(10);
  if(Wire.available())
  {
    byte r = Wire.read();
    Serial.print("Received from digipot: "); Serial.println(r, BIN);
    return r;
  }
  return -1;
}

bool Lid::_set_current(uint8_t data)
{
  // first wiper address on digi-pot is at location 0x00
  if(!_i2c_write(AD5110_SET_VALUE_CMD, data))
  {
    return false;
  }
  delay(SET_CURRENT_DELAY_MS);
  return true;
}

bool Lid::_save_current()
{
  if(!_i2c_write(AD5110_SAVE_VALUE_CMD, 0x00))
  {
    return false;
  }
  delay(SET_CURRENT_DELAY_MS);
  return true;
}

float Lid::_read_tolerance()
{
  _i2c_write(AD5110_READ_TOLERANCE_CMD, 0x01);
  delay(30);
  byte tol_byte = _i2c_read();
  byte integer = ((tol_byte & 0b01111000) >> 3);
  byte fraction = tol_byte & 0b00000111;
  byte sign = ((tol_byte & 0b10000000) >> 7);
  if (sign == 0)
  {
    sign = -1;
  }
  else
  {
    sign = 1;
  }
  float tol = sign * (integer + ((fraction & 0x04) >> 2) * 0.5 +
                                ((fraction & 0x02) >> 1) * 0.25 +
                                ((fraction & 0x01) >> 0) * 0.125);
  return tol;
}

bool Lid::_setup_digipot()
{
  Wire.begin();
  // make sure that on power-up, the digi-pot sets the current to 0.0amps
  if(_set_current(0.0) && _save_current())
  {
    return true;
  }
  return false;
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
      _status = Lid_status::unknown;
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

bool Lid::is_driver_faulted()
{
#if HW_VERSION >= 3
  return motor_driver_faulted;
#endif
  return false;
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
  delayMicroseconds(MOTOR_STEP_DELAY);
}

uint16_t Lid::_to_dac_out(float driver_vref)
{
  return uint16_t(driver_vref * float(1023/3.3));
}

void Lid::reset_motor_driver()
{
#if HW_VERSION >= 3
  digitalWrite(PIN_MOTOR_RST, LOW);
  delay(100);
  digitalWrite(PIN_MOTOR_RST, HIGH);
#endif
}

bool Lid::move_angle(float deg)
{
  uint8_t dir = DIRECTION_UP;
  if (deg < 0) dir = DIRECTION_DOWN;
  unsigned long steps = abs(deg) * STEPS_PER_ANGLE;

  for (unsigned long i=0;i<steps;i++)
  {
    _motor_step(dir);
    check_switches();
  #if LID_TESTING
    if (Serial.available())
    {
      return false;
    }
  #endif
    if (dir == DIRECTION_UP)
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
        delay(50);
        for (int j = 0; j < LID_CLOSE_BACKTRACK_STEPS; j++)
        {
          // Backtrack a bit
          _motor_step(DIRECTION_UP);
        }
        return true;
      }
    }
  }
  return false;
}

bool Lid::open_cover()
{
  if (_is_cover_switch_pressed)
  {
     return true;
  }
  motor_on();
  move_angle(-1);    // move down a bit
  solenoid_on();
  delay(100);
  move_angle(5);     // move up a bit
  solenoid_off();
  bool res = move_angle(LID_MOTOR_RANGE_DEG);
  motor_off();
  return res;
}

bool Lid::close_cover()
{
  if (_is_bottom_switch_pressed)
  {
    return true;
  }
  // solenoid_on();
  motor_on();
  bool res = move_angle(-LID_MOTOR_RANGE_DEG);
  solenoid_off();
  delay(700);
  motor_off();
  return res;
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

void _motor_fault_callback()
{
#if HW_VERSION >= 3
  if(digitalRead(PIN_MOTOR_FAULT) == LOW)
  {
    motor_driver_faulted = true;
  }
#endif
}

bool Lid::setup()
{
  bool status = true;
	pinMode(PIN_SOLENOID, OUTPUT);
	solenoid_off();
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_ENABLE, OUTPUT);
#if HW_VERSION >= 3
  pinMode(PIN_MOTOR_FAULT, INPUT_PULLUP);
  pinMode(PIN_MOTOR_RST, OUTPUT);
  digitalWrite(PIN_MOTOR_RST, HIGH);
  attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_FAULT), _motor_fault_callback, FALLING);
  // Use DAC to set Vref for motor current limit
  analogWriteResolution(10);
  analogWrite(PIN_MOTOR_CURRENT_VREF, _to_dac_out(MOTOR_CURRENT_VREF));
  analogWriteResolution(8);
  status = true;
#else
  // No fault detection
  // Digipot used for stepper control Vref for motor current limit
  if(!_setup_digipot())
  {
    status = false;
  }
#endif
	motor_off();
  _set_current(CURRENT_SETTING);
  _save_current();
#if DUMMY_BOARD
  pinMode(PIN_COVER_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BOTTOM_SWITCH, INPUT_PULLUP);
#else
  pinMode(PIN_COVER_SWITCH, INPUT);
  pinMode(PIN_BOTTOM_SWITCH, INPUT);
#endif
  // Both switches are NORMALLY CLOSED
  _is_cover_switch_pressed = bool(digitalRead(PIN_COVER_SWITCH));
  _is_bottom_switch_pressed = bool(digitalRead(PIN_BOTTOM_SWITCH));
  _update_status();
  attachInterrupt(digitalPinToInterrupt(PIN_COVER_SWITCH), _cover_switch_callback, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BOTTOM_SWITCH), _bottom_switch_callback, CHANGE);
  return status;
}
