#ifndef Thermistor_h
#define Thermistor_h

#include "Arduino.h"

#define thermistor_pin 5
#define TABLE_SIZE 21

class Thermistor{
  public:

    Thermistor();
    float peltier_temperature(float avg_adc=-1);
    float average_adc(int pin=-1, int numsamples=5);

  private:

    const unsigned int TABLE[TABLE_SIZE][3] = {  // ADC, Celsius, Ohms
      {781, 0, 32330},
      {732, 5, 25194},
      {680, 10, 19785},
      {624, 15, 15651},
      {568, 20, 12468},
      {512, 25, 10000},
      {457, 30, 8072},
      {405, 35, 6556},
      {357, 40, 5356},
      {313, 45, 4401},
      {273, 50, 3635},
      {237, 55, 3019},
      {206, 60, 2521},
      {179, 65, 2115},
      {155, 70, 1781},
      {134, 75, 1509},
      {116, 80, 1284},
      {101, 85, 1097},
      {88, 90, 941},
      {77, 95, 810},
      {67, 100, 701}
    };

};

#endif
