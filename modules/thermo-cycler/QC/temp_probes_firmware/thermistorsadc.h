#ifndef ThermistorsADC_h
#define ThermistorsADC_h

#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_ADS1015.h>

#define ADC_TABLE_SIZE 156
#define ADC_PER_DEVICE 4

#define ADDRESS_A 0x48
#define ADDRESS_B 0x49
#define ADDRESS_C 0x4A
#define ADDRESS_D 0x4B

#define TOTAL_GAIN_SETTINGS 6

#define GAIN_TWOTHIRDS_VOLTAGE   6.144
#define GAIN_ONE_VOLTAGE         4.096
#define GAIN_TWO_VOLTAGE         2.048
#define GAIN_FOUR_VOLTAGE        1.024
#define GAIN_EIGHT_VOLTAGE       0.512
#define GAIN_SIXTEEN_VOLTAGE     0.256

class ThermistorsADC{
      public:

            ThermistorsADC();
            void begin();
            void begin(float voltage);
            float temperature(int index);

      private:

            float voltage = 5.0;

            adsGain_t gain_settings[6] = {
                  GAIN_TWOTHIRDS,
                  GAIN_ONE,
                  GAIN_TWO,
                  GAIN_FOUR,
                  GAIN_EIGHT,
                  GAIN_SIXTEEN
            };

            float gain_max_voltage[6] = {
                  GAIN_TWOTHIRDS_VOLTAGE,
                  GAIN_ONE_VOLTAGE,
                  GAIN_TWO_VOLTAGE,
                  GAIN_FOUR_VOLTAGE,
                  GAIN_EIGHT_VOLTAGE,
                  GAIN_SIXTEEN_VOLTAGE
            };

            Adafruit_ADS1115 *adc_a;
            Adafruit_ADS1115 *adc_b;
            Adafruit_ADS1115 *adc_c;
            Adafruit_ADS1115 *adc_d;

            int _read_adc(int index);
            float _adc_to_celsius(int _adc);

            // lookup table provided for thermistor PN: KS103J2
            // ADC values calculated for when powering 1.5 volts into
            // a 10k ohm resistor, followed by the thermistor leading to GND
            const int TABLE[ADC_TABLE_SIZE][2] = {
                  // ADC, Celsius
                  {21758, -20},
                  {21638, -19},
                  {21512, -18},
                  {21382, -17},
                  {21247, -16},
                  {21106, -15},
                  {20961, -14},
                  {20810, -13},
                  {20654, -12},
                  {20492, -11},
                  {20326, -10},
                  {20154, -9},
                  {19976, -8},
                  {19794, -7},
                  {19606, -6},
                  {19413, -5},
                  {19215, -4},
                  {19011, -3},
                  {18803, -2},
                  {18590, -1},
                  {18372, 0},
                  {18150, 1},
                  {17923, 2},
                  {17692, 3},
                  {17457, 4},
                  {17218, 5},
                  {16976, 6},
                  {16730, 7},
                  {16480, 8},
                  {16228, 9},
                  {15973, 10},
                  {15715, 11},
                  {15455, 12},
                  {15194, 13},
                  {14930, 14},
                  {14665, 15},
                  {14399, 16},
                  {14132, 17},
                  {13865, 18},
                  {13597, 19},
                  {13329, 20},
                  {13062, 21},
                  {12795, 22},
                  {12529, 23},
                  {12263, 24},
                  {12000, 25},
                  {11737, 26},
                  {11477, 27},
                  {11218, 28},
                  {10962, 29},
                  {10708, 30},
                  {10457, 31},
                  {10208, 32},
                  {9963, 33},
                  {9721, 34},
                  {9481, 35},
                  {9246, 36},
                  {9014, 37},
                  {8785, 38},
                  {8561, 39},
                  {8340, 40},
                  {8123, 41},
                  {7910, 42},
                  {7701, 43},
                  {7496, 44},
                  {7296, 45},
                  {7099, 46},
                  {6907, 47},
                  {6719, 48},
                  {6535, 49},
                  {6355, 50},
                  {6179, 51},
                  {6008, 52},
                  {5841, 53},
                  {5677, 54},
                  {5518, 55},
                  {5363, 56},
                  {5211, 57},
                  {5064, 58},
                  {4920, 59},
                  {4781, 60},
                  {4645, 61},
                  {4512, 62},
                  {4383, 63},
                  {4258, 64},
                  {4136, 65},
                  {4018, 66},
                  {3903, 67},
                  {3791, 68},
                  {3682, 69},
                  {3577, 70},
                  {3475, 71},
                  {3375, 72},
                  {3278, 73},
                  {3185, 74},
                  {3094, 75},
                  {3005, 76},
                  {2919, 77},
                  {2836, 78},
                  {2755, 79},
                  {2677, 80},
                  {2601, 81},
                  {2527, 82},
                  {2455, 83},
                  {2386, 84},
                  {2319, 85},
                  {2254, 86},
                  {2191, 87},
                  {2129, 88},
                  {2070, 89},
                  {2012, 90},
                  {1957, 91},
                  {1903, 92},
                  {1850, 93},
                  {1799, 94},
                  {1750, 95},
                  {1702, 96},
                  {1656, 97},
                  {1611, 98},
                  {1567, 99},
                  {1525, 100},
                  {1484, 101},
                  {1444, 102},
                  {1405, 103},
                  {1368, 104},
                  {1331, 105},
                  {1296, 106},
                  {1262, 107},
                  {1229, 108},
                  {1197, 109},
                  {1165, 110},
                  {1135, 111},
                  {1106, 112},
                  {1077, 113},
                  {1049, 114},
                  {1023, 115},
                  {996, 116},
                  {971, 117},
                  {946, 118},
                  {923, 119},
                  {899, 120},
                  {877, 121},
                  {855, 122},
                  {834, 123},
                  {813, 124},
                  {793, 125},
                  {773, 126},
                  {754, 127},
                  {736, 128},
                  {718, 129},
                  {701, 130},
                  {684, 131},
                  {667, 132},
                  {652, 133},
                  {636, 134},
                  {621, 135}
            };

};

#endif
