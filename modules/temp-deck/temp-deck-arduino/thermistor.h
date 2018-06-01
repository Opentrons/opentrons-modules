#ifndef Thermistor_h
#define Thermistor_h

#include "Arduino.h"

#define thermistor_pin 5
#define TABLE_SIZE 34

#define THERMISTOR_NUM_SAMPLES 15

class Thermistor{
  public:

    Thermistor();
    bool update();
    float plate_temperature();
    void set_samples(int n);

  private:

    uint8_t sample_index = 0;
    int samples[THERMISTOR_NUM_SAMPLES];
    float average = 0;

    float _average_adc();
    float _thermistor_temp_to_plate_temp(float thermistor_temp);

    // lookup table provided for thermistor PN: NXFT15XV103FA2B150
    const unsigned int TABLE[TABLE_SIZE][2] = {
      // ADC, Celsius
      {994, -40},
      {983, -35},
      {968, -30},
      {950, -25},
      {928, -20},
      {900, -15},
      {865, -10},
      {826, -5},
      {781, 0},
      {732, 5},
      {680, 10},
      {624, 15},
      {568, 20},
      {512, 25},
      {457, 30},
      {405, 35},
      {357, 40},
      {313, 45},
      {273, 50},
      {237, 55},
      {206, 60},
      {179, 65},
      {155, 70},
      {134, 75},
      {116, 80},
      {101, 85},
      {88, 90},
      {77, 95},
      {67, 100},
      {44, 115},
      {38, 120},
      {34, 125}
    };

};

#endif
