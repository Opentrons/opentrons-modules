//Controls Thermoelectric, peltier modules
//Peliter_A and Pelteir_B are wired in series
//H-Bridges -> Controls the direction of current (hot or cold)

#ifndef Peltiers_h
#define Peltiers_h

#include "Arduino.h"

#define PEL_INT(pel)  static_cast<int>(pel)

enum class Peltier
{
    no_peltier=-1,
    pel_1=0,
    pel_2,
    pel_3,
    max_num
};

#define PIN_PELTIER_CONTROL_1A       5   // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_1B       11  // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_2A       13  // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_2B       10  // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_3A       12  // uses PWM frequency generator
#define PIN_PELTIER_CONTROL_3B       6   // uses PWM frequency generator
#define PIN_PELTIER_ENABLE           7

#define SINGLE_PID  false

typedef struct Peltier_property
{
    int pin_a;
    int pin_b;
    int prev_val_a;
    int prev_val_b;
};

class Peltiers
{
  public:

    Peltiers();
    void setup();
    void disable();
    void set_cold_percentage(double perc, Peltier n);
    void set_hot_percentage(double perc, Peltier n);

  private:
    Peltier_property pel[PEL_INT(Peltier::max_num)];
};

#endif
