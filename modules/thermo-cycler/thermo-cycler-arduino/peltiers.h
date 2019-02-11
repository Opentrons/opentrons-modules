//Controls Thermoelectric, peltier modules
//Peliter_A and Pelteir_B are wired in series
//H-Bridges -> Controls the direction of current (hot or cold)

#ifndef Peltiers_h
#define Peltiers_h

#include "Arduino.h"

#define PIN_PELTIER_CONTROL_A       A4   // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_B       6   // uses PWM frequency generator
#define PIN_PELTIER_ENABLE          12

class Peltiers{
  public:

    Peltiers();
    void setup();
    void disable();
    void set_cold_percentage(double perc);
    void set_hot_percentage(double perc);

  private:

    int prev_val_a = 0;
    int prev_val_b = 0;

    uint8_t peltier_a_control;
    uint8_t peltier_b_control;
    uint8_t peltier_ab_enable;
};

#endif
