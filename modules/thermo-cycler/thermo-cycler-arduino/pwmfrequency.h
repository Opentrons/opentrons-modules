#ifndef Pwmfrequency_h
#define Pwmfrequency_h

#include "Arduino.h"

#define ALLOWED_PIN_TWO        2
#define ALLOWED_PIN_FIVE       5
#define ALLOWED_PIN_SIX        6
#define ALLOWED_PIN_SEVEN      7

#define DEFAULT_PWM_FREQ       250

class Pwmfrequency{
  public:

    Pwmfrequency();
    void pwm_with_frequency(int pin, double duty, double freq_k_hz=DEFAULT_PWM_FREQ);

  private:
};

#endif
