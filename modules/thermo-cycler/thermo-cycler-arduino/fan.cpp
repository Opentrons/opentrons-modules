#include "fan.h"
#include "high_frequency_pwm.h"

Fan::Fan()
{}

/* Only for the pwm controlled heatsink fan */
void Fan::set_percentage(float p)
{
  enable();
  p = constrain(p, 0, 1);
  uint8_t val = p * 255.0;
  current_power = p;
#if HFQ_PWM
  hfq_analogWrite(_pwm_pin, val);
#else
  analogWrite(_pwm_pin, val);
#endif
}

/* Only for the pwm controlled heatsink fan */
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
