//Controls Thermoelectric, peltier modules
//Peliter_A and Pelteir_B are wired in series
//H-Bridges -> Controls the direction of current (hot or cold)

#ifndef Peltiers_h
#define Peltiers_h

#include "Arduino.h"

#define PELTIER_A_CONTROL 13
#define PELTIER_B_CONTROL 10
#define PELTIER_AB_ENABLE 8

#define DEFAULT_PELTIER_CYCLE_MS 100

class Peltiers{
  public:

    Peltiers();
    void setup_peltiers();
    void update_peltier_cycle();
    void disable_peltiers();
    void set_cold_percentage(float perc);
    void set_hot_percentage(float perc);

  private:

    int peltier_on_time;
    int peltier_off_time;
    int peltier_high_pin;
    int peltier_low_pin;
    boolean enabled = false;
    boolean peltiers_currently_on = false;

    const float PELTIER_CYCLE_MS = 100;
    unsigned long peltier_cycle_timestamp = 0;
    unsigned long now = 0;

    void set_peltiers_percentage(float a_state, float b_state);
    void write_h_bridges(int state);

};

#endif
