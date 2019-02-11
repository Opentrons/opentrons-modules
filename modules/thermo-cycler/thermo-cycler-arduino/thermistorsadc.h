#ifndef ThermistorsADC_h
#define ThermistorsADC_h

#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_ADS1015.h>

#define ADC_TABLE_SIZE 291
#define ADC_PER_DEVICE 4

#define ADDRESS_A 0x48
#define ADDRESS_B 0x49

#define TOTAL_GAIN_SETTINGS 6

#define GAIN_TWOTHIRDS_VOLTAGE   6.144
#define GAIN_ONE_VOLTAGE         4.096
#define GAIN_TWO_VOLTAGE         2.048
#define GAIN_FOUR_VOLTAGE        1.024
#define GAIN_EIGHT_VOLTAGE       0.512
#define GAIN_SIXTEEN_VOLTAGE     0.256

#define ADC_INDEX_HEAT_SINK            0
#define ADC_INDEX_PLATE_FRONT_RIGHT    1  //Bottom Right on sch.
#define ADC_INDEX_PLATE_FRONT_CENTER   2  //Bottom Center on sch.
#define ADC_INDEX_PLATE_FRONT_LEFT     3  //Bottom Lef on sch.
#define ADC_INDEX_PLATE_BACK_LEFT      4  //Top Left on sch.
#define ADC_INDEX_PLATE_BACK_CENTER    5  //Top Center on sch.
#define ADC_INDEX_PLATE_BACK_RIGHT     6  //Top Right on sch.


#define ADC_INDEX_COVER                7

#define TOTAL_THERMISTORS              8
#define TOTAL_PLATE_THERMISTORS        6

class ThermistorsADC{
      public:

            ThermistorsADC();
            void setup(float voltage);
            bool update();

            float average_plate_temperature();
            float left_pair_temperature();
            float center_pair_temperature();
            float right_pair_temperature();
            float front_left_temperature();
            float front_center_temperature();
            float front_right_temperature();
            float back_left_temperature();
            float back_center_temperature();
            float back_right_temperature();
            float cover_temperature();
            float heat_sink_temperature();

      private:

            Adafruit_ADS1115 *adc_a;
            Adafruit_ADS1115 *adc_b;

            int _read_adc(int index);
            float _adc_to_celsius(int _adc);

            double probe_temps[TOTAL_THERMISTORS] = {0, };
            double sum_probe_temps[TOTAL_THERMISTORS] = {0, };
            double probe_sample_count = 0;

            const int temp_read_interval = 100;
            const int inter_temp_read_interval = 1;
            unsigned long temp_read_timestamp = 0;

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
                  {18262, 0.5},
                  {18150, 1},
                  {18037, 1.5},
                  {17923, 2},
                  {17808, 2.5},
                  {17692, 3},
                  {17575, 3.5},
                  {17457, 4},
                  {17338, 4.5},
                  {17218, 5},
                  {17097, 5.5},
                  {16976, 6},
                  {16853, 6.5},
                  {16730, 7},
                  {16605, 7.5},
                  {16480, 8},
                  {16354, 8.5},
                  {16228, 9},
                  {16101, 9.5},
                  {15973, 10},
                  {15844, 10.5},
                  {15715, 11},
                  {15586, 11.5},
                  {15455, 12},
                  {15325, 12.5},
                  {15194, 13},
                  {15062, 13.5},
                  {14930, 14},
                  {14798, 14.5},
                  {14665, 15},
                  {14532, 15.5},
                  {14399, 16},
                  {14266, 16.5},
                  {14132, 17},
                  {13999, 17.5},
                  {13865, 18},
                  {13731, 18.5},
                  {13597, 19},
                  {13463, 19.5},
                  {13329, 20},
                  {13195, 20.5},
                  {13062, 21},
                  {12928, 21.5},
                  {12795, 22},
                  {12662, 22.5},
                  {12529, 23},
                  {12396, 23.5},
                  {12263, 24},
                  {12131, 24.5},
                  {12000, 25},
                  {11868, 25.5},
                  {11737, 26},
                  {11607, 26.5},
                  {11477, 27},
                  {11347, 27.5},
                  {11218, 28},
                  {11090, 28.5},
                  {10962, 29},
                  {10835, 29.5},
                  {10708, 30},
                  {10582, 30.5},
                  {10457, 31},
                  {10332, 31.5},
                  {10208, 32},
                  {10085, 32.5},
                  {9963, 33},
                  {9841, 33.5},
                  {9721, 34},
                  {9601, 34.5},
                  {9481, 35},
                  {9363, 35.5},
                  {9246, 36},
                  {9129, 36.5},
                  {9014, 37},
                  {8899, 37.5},
                  {8785, 38},
                  {8673, 38.5},
                  {8561, 39},
                  {8450, 39.5},
                  {8340, 40},
                  {8231, 40.5},
                  {8123, 41},
                  {8016, 41.5},
                  {7910, 42},
                  {7805, 42.5},
                  {7701, 43},
                  {7598, 43.5},
                  {7496, 44},
                  {7396, 44.5},
                  {7296, 45},
                  {7197, 45.5},
                  {7099, 46},
                  {7003, 46.5},
                  {6907, 47},
                  {6812, 47.5},
                  {6719, 48},
                  {6626, 48.5},
                  {6535, 49},
                  {6444, 49.5},
                  {6355, 50},
                  {6267, 50.5},
                  {6179, 51},
                  {6093, 51.5},
                  {6008, 52},
                  {5924, 52.5},
                  {5841, 53},
                  {5758, 53.5},
                  {5677, 54},
                  {5597, 54.5},
                  {5518, 55},
                  {5440, 55.5},
                  {5363, 56},
                  {5287, 56.5},
                  {5211, 57},
                  {5137, 57.5},
                  {5064, 58},
                  {4992, 58.5},
                  {4920, 59},
                  {4850, 59.5},
                  {4781, 60},
                  {4712, 60.5},
                  {4645, 61},
                  {4578, 61.5},
                  {4512, 62},
                  {4447, 62.5},
                  {4383, 63},
                  {4320, 63.5},
                  {4258, 64},
                  {4197, 64.5},
                  {4136, 65},
                  {4077, 65.5},
                  {4018, 66},
                  {3960, 66.5},
                  {3903, 67},
                  {3847, 67.5},
                  {3791, 68},
                  {3736, 68.5},
                  {3682, 69},
                  {3629, 69.5},
                  {3577, 70},
                  {3525, 70.5},
                  {3475, 71},
                  {3424, 71.5},
                  {3375, 72},
                  {3326, 72.5},
                  {3278, 73},
                  {3231, 73.5},
                  {3185, 74},
                  {3139, 74.5},
                  {3094, 75},
                  {3049, 75.5},
                  {3005, 76},
                  {2962, 76.5},
                  {2919, 77},
                  {2877, 77.5},
                  {2836, 78},
                  {2795, 78.5},
                  {2755, 79},
                  {2716, 79.5},
                  {2677, 80},
                  {2638, 80.5},
                  {2601, 81},
                  {2563, 81.5},
                  {2527, 82},
                  {2491, 82.5},
                  {2455, 83},
                  {2421, 83.5},
                  {2386, 84},
                  {2352, 84.5},
                  {2319, 85},
                  {2286, 85.5},
                  {2254, 86},
                  {2222, 86.5},
                  {2191, 87},
                  {2160, 87.5},
                  {2129, 88},
                  {2099, 88.5},
                  {2070, 89},
                  {2041, 89.5},
                  {2012, 90},
                  {1984, 90.5},
                  {1957, 91},
                  {1929, 91.5},
                  {1903, 92},
                  {1876, 92.5},
                  {1850, 93},
                  {1824, 93.5},
                  {1799, 94},
                  {1774, 94.5},
                  {1750, 95},
                  {1726, 95.5},
                  {1702, 96},
                  {1679, 96.5},
                  {1656, 97},
                  {1633, 97.5},
                  {1611, 98},
                  {1589, 98.5},
                  {1567, 99},
                  {1546, 99.5},
                  {1525, 100},
                  {1504, 100.5},
                  {1484, 101},
                  {1464, 101.5},
                  {1444, 102},
                  {1424, 102.5},
                  {1405, 103},
                  {1386, 103.5},
                  {1368, 104},
                  {1349, 104.5},
                  {1331, 105},
                  {1314, 105.5},
                  {1296, 106},
                  {1279, 106.5},
                  {1262, 107},
                  {1245, 107.5},
                  {1229, 108},
                  {1213, 108.5},
                  {1197, 109},
                  {1181, 109.5},
                  {1165, 110},
                  {1150, 110.5},
                  {1135, 111},
                  {1120, 111.5},
                  {1106, 112},
                  {1091, 112.5},
                  {1077, 113},
                  {1063, 113.5},
                  {1049, 114},
                  {1036, 114.5},
                  {1023, 115},
                  {1009, 115.5},
                  {996, 116},
                  {984, 116.5},
                  {971, 117},
                  {959, 117.5},
                  {946, 118},
                  {934, 118.5},
                  {923, 119},
                  {911, 119.5},
                  {899, 120},
                  {888, 120.5},
                  {877, 121},
                  {866, 121.5},
                  {855, 122},
                  {844, 122.5},
                  {834, 123},
                  {823, 123.5},
                  {813, 124},
                  {803, 124.5},
                  {793, 125},
                  {783, 125.5},
                  {773, 126},
                  {764, 126.5},
                  {754, 127},
                  {745, 127.5},
                  {736, 128},
                  {727, 128.5},
                  {718, 129},
                  {709, 129.5},
                  {701, 130},
                  {692, 130.5},
                  {684, 131},
                  {676, 131.5},
                  {667, 132},
                  {659, 132.5},
                  {652, 133},
                  {644, 133.5},
                  {636, 134},
                  {628, 134.5},
                  {621, 135}
            };
};

#endif
