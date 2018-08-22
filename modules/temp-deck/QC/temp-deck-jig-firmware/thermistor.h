#ifndef Thermistor_h
#define Thermistor_h

#include "Arduino.h"

#define thermistor_pin 0
#define TABLE_SIZE 22

#define THERMISTOR_NUM_SAMPLES 15

class Thermistor{
  public:

    Thermistor();
    bool update();
    float temperature();
    void set_samples(int n);

  private:

    uint8_t sample_index = 0;
    int samples[THERMISTOR_NUM_SAMPLES];

    void _calculate_average_adc();
    float _average_adc;

    // lookup table provided for thermistor PN: KS103J2
    const int TABLE[TABLE_SIZE][2] = {
      // ADC, Celsius
      {827, -5},
      {783, 0},
      {734, 5},
      {681, 10},
      {625, 15},
      {568, 20},
      {512, 25},
      {456, 30},
      {404, 35},
      {356, 40},
      {311, 45},
      {271, 50},
      {235, 55},
      {204, 60},
      {176, 65},
      {152, 70},
      {132, 75},
      {114, 80},
      {99, 85},
      {86, 90},
      {75, 95},
      {65, 100}
    };

};

#endif
