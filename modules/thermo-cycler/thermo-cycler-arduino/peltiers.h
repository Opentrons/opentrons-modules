//Controls Thermoelectric, peltier modules
//Peliter_A and Pelteir_B are wired in series
//H-Bridges -> Controls the direction of current (hot or cold)

#ifndef Peltiers_h
#define Peltiers_h

#include "Arduino.h"

typedef enum{
    NO_PELTIER=-1,
    PELTIER_1=0,
    PELTIER_2,
    PELTIER_3,
}Peltier_num;

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

class Peltiers{
  public:

    Peltiers();
    void setup();
    void disable();
    void set_cold_percentage(double perc, Peltier_num n);
    void set_hot_percentage(double perc, Peltier_num n);

  private:
    Peltier_property pel[3];

    uint8_t peltier_1a_control;
    uint8_t peltier_1b_control;
    uint8_t peltier_2a_control;
    uint8_t peltier_2b_control;
    uint8_t peltier_3a_control;
    uint8_t peltier_3b_control;
    uint8_t peltiers_enable;
};

#endif
