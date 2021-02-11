#include "fan.h"
#include "high_frequency_pwm.h"

Fan::Fan()
{}

/* Only for the pwm controlled heatsink fan */
void Fan::set_percentage(float p, Fan_ramping r)
{
  enable();
  p = constrain(p, 0, 1);
  if (r == Fan_ramping::On)
  {
    const float alpha = 0.1;
    // Low pass filter to dampen sudden changes in fan speed.
    // Provides better user experience.
    current_power = current_power + alpha * (p - current_power);
  }
  else
  {
    current_power = p;
  }
  uint8_t val = current_power * 255.0;
#if HFQ_PWM
  hfq_analogWrite(_pwm_pin, val);
#else
  analogWrite(_pwm_pin, val);
#endif
}

void Fan::set_percentage(float p)
{
  set_percentage(p, Fan_ramping::Off);
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
