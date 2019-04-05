#include "fan.h"
#include "wiring_private.h"
#include "high_frequency_pwm.h"

Fan::Fan()
{}

void Fan::set_percentage(float p)
{
  enable();
  uint8_t val = p * 255.0;
  if (val < 0)
  {
    val = 0;
  }
  else if (val > 255)
  {
    val = 255;
  }
  current_power = p;
  hfq_analogWrite(_pwm_pin, val);
}

void Fan::setup_pwm_pin(uint8_t pwm_pin)
{
  _pwm_pin = pwm_pin;
  _pwm_controlled = true;
}

void Fan::setup_enable_pin(uint8_t enable_pin, bool active_high)
{
  pinMode(enable_pin, OUTPUT);
  digitalWrite(enable_pin, int(!active_high));
  _enable_pin = enable_pin;
  _active_high = active_high;
}

void Fan::enable()
{
  digitalWrite(_enable_pin, int(_active_high));
}

void Fan::disable()
{
  if (_pwm_controlled)
  {
    set_percentage(0);
  }
  digitalWrite(_enable_pin, int(!_active_high));
}
