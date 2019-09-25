#include "thermistorsadc.h"

ThermistorsADC::ThermistorsADC() {}

void ThermistorsADC::setup(float voltage) {
  adc_a = new Adafruit_ADS1115(ADDRESS_A);
  adc_b = new Adafruit_ADS1115(ADDRESS_B);

  adsGain_t gain = gain_settings[0];  // safest gain by default
  for (uint8_t i=1;i<TOTAL_GAIN_SETTINGS;i++) {
    if (voltage < gain_max_voltage[i]) {
      gain = gain_settings[i];
    }
  }

  adc_a->setGain(gain);
  adc_b->setGain(gain);

  adc_a->begin();
  adc_b->begin();

  adc_a->setSPS(ADS1115_DR_250SPS);   // Sample rate 250 per sec
  adc_b->setSPS(ADS1115_DR_250SPS);   // Sample rate 250 per sec
}

void ThermistorsADC::update(ThermistorPair n) {
  uint8_t therm_index1, therm_index2;
  switch(n)
  {
    case ThermistorPair::left:
      therm_index1 = ADC_INDEX_PLATE_FRONT_LEFT;
      therm_index2 = ADC_INDEX_PLATE_BACK_LEFT;
      break;
    case ThermistorPair::center:
      therm_index1 = ADC_INDEX_PLATE_FRONT_CENTER;
      therm_index2 = ADC_INDEX_PLATE_BACK_CENTER;
      break;
    case ThermistorPair::right:
      therm_index1 = ADC_INDEX_PLATE_FRONT_RIGHT;
      therm_index2 = ADC_INDEX_PLATE_BACK_RIGHT;
      break;
    case ThermistorPair::cover_n_heatsink:
      therm_index1 = ADC_INDEX_HEAT_SINK;
      therm_index2 = ADC_INDEX_COVER;
      break;
  }
  probe_temps[therm_index1] = _adc_to_celsius(_read_adc(therm_index1));
  delay(inter_temp_read_interval);
  probe_temps[therm_index2] = _adc_to_celsius(_read_adc(therm_index2));
  delay(inter_temp_read_interval);
  if (!is_in_range(probe_temps[therm_index1]) || !is_in_range(probe_temps[therm_index2]))
  {
    // Once set to true, will need system reset to clear
    detected_invalid_val = true;
  }
}

bool ThermistorsADC::is_in_range(double celsius)
{
  return (celsius < THERMISTOR_ERROR_VAL_HI && celsius > THERMISTOR_ERROR_VAL_LOW);
}

float ThermistorsADC::average_plate_temperature() {
  return (
    front_left_temperature() +
    front_center_temperature() +
    front_right_temperature() +
    back_left_temperature() +
    back_center_temperature() +
    back_right_temperature()
  ) / TOTAL_PLATE_THERMISTORS;
}

float ThermistorsADC::left_pair_temperature() {
  return (front_left_temperature() + back_left_temperature())/2;
}

float ThermistorsADC::center_pair_temperature() {
  return (front_center_temperature() + back_center_temperature())/2;
}

float ThermistorsADC::right_pair_temperature() {
  return (front_right_temperature() + back_right_temperature())/2;
}

float ThermistorsADC::front_left_temperature() {
  return probe_temps[ADC_INDEX_PLATE_FRONT_LEFT] + plate_temp_offset;
}

float ThermistorsADC::front_center_temperature() {
  return probe_temps[ADC_INDEX_PLATE_FRONT_CENTER] + plate_temp_offset;
}

float ThermistorsADC::front_right_temperature() {
  return probe_temps[ADC_INDEX_PLATE_FRONT_RIGHT] + plate_temp_offset;
}

float ThermistorsADC::back_left_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_LEFT] + plate_temp_offset;
}

float ThermistorsADC::back_center_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_CENTER] + plate_temp_offset;
}

float ThermistorsADC::back_right_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_RIGHT] + plate_temp_offset;
}

float ThermistorsADC::cover_temperature() {
  return probe_temps[ADC_INDEX_COVER];  // No offset
}

float ThermistorsADC::heat_sink_temperature() {
  return probe_temps[ADC_INDEX_HEAT_SINK];  // No offset
}


uint16_t ThermistorsADC::_read_adc(int index) {
  int res = 0;
  switch (int(index / ADC_PER_DEVICE)) {
    case 0:
      res = adc_a->readADC_SingleEnded(index % ADC_PER_DEVICE);
      break;
    case 1:
      res = adc_b->readADC_SingleEnded(index % ADC_PER_DEVICE);
      break;
  }
  if (res >=0)
  {
    return uint16_t(res);
  }
  return uint16_t(0);
}

float ThermistorsADC::_adc_to_celsius(uint16_t _adc) {
  if (_adc < TABLE[ADC_TABLE_SIZE-1].adc_reading) {
    return TABLE[ADC_TABLE_SIZE-1].celsius;
  }
  else if (_adc > TABLE[0].adc_reading){
    return TABLE[0].celsius;
  }
  else {
    int ADC_LOW, ADC_HIGH;
    for (int i=0;i<ADC_TABLE_SIZE - 1;i++){
      ADC_HIGH = TABLE[i].adc_reading;
      ADC_LOW = TABLE[i+1].adc_reading;
      if (_adc >= ADC_LOW && _adc <= ADC_HIGH){
        float p = float(abs(ADC_HIGH - _adc)) / abs(ADC_HIGH - ADC_LOW);
        p *= float(abs(TABLE[i+1].celsius - TABLE[i].celsius));
        p += float(TABLE[i].celsius);
        return p;
      }
    }
  }
}

const AdcToCelsius ThermistorsADC::TABLE[] = {
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
