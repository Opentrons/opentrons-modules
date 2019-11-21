#ifndef fan_h
#define fan_h

#include "Arduino.h"

#define ENABLE_DEFAULT_ACTIVE_HIGH  true

enum class Fan_ramping
{
  On,
  Off
};

/* This class is used to control the two fans in the thermocycler:
 *  - Cover heatpad fan: Digital control (ON/OFF only)
 *  - Heatsink fan: PWM control + ON/OFF control */
class Fan
{
  public:
    Fan();
    void set_percentage(float p);
    void set_percentage(float p, Fan_ramping r);
    void setup_pwm_pin(uint8_t pwm_pin);
    void setup_enable_pin(uint8_t enable_pin, bool active_high);
    void enable();
    void disable();
    float current_power;
    float manual_power;

  private:
    uint8_t _pwm_pin;
    uint8_t _enable_pin;
    bool _pwm_controlled;
    bool _active_high = ENABLE_DEFAULT_ACTIVE_HIGH;
};
#endif
