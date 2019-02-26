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
}

bool ThermistorsADC::update() {
  for (uint8_t i=0;i<TOTAL_THERMISTORS;i++){
    sum_probe_temps[i] += _adc_to_celsius(_read_adc(i));
    // small delay found to help avoid I2C read errors
    delay(inter_temp_read_interval);
  }
  probe_sample_count += 1;
  if (millis() - temp_read_timestamp > temp_read_interval) {
    for (uint8_t i=0;i<TOTAL_THERMISTORS;i++){
      probe_temps[i] = sum_probe_temps[i] / probe_sample_count;
      sum_probe_temps[i] = 0;
    }
    probe_sample_count = 0;
    return true;
  }
  return false;
}

float ThermistorsADC::average_plate_temperature() {
  return (
    front_left_temperature() +
    front_center_temperature() +
    front_right_temperature() +
    back_left_temperature() +
    back_center_temperature() +
    back_right_temperature() +
    //heat_sink_temperature() +
    0.0
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
  return probe_temps[ADC_INDEX_PLATE_FRONT_LEFT];
}

float ThermistorsADC::front_center_temperature() {
  return probe_temps[ADC_INDEX_PLATE_FRONT_CENTER];
}

float ThermistorsADC::front_right_temperature() {
  return probe_temps[ADC_INDEX_PLATE_FRONT_RIGHT];
}

float ThermistorsADC::back_left_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_LEFT];
}

float ThermistorsADC::back_center_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_CENTER];
}

float ThermistorsADC::back_right_temperature() {
  return probe_temps[ADC_INDEX_PLATE_BACK_RIGHT];
}

float ThermistorsADC::cover_temperature() {
  return probe_temps[ADC_INDEX_COVER];
}

float ThermistorsADC::heat_sink_temperature() {
  return probe_temps[ADC_INDEX_HEAT_SINK];
}


int ThermistorsADC::_read_adc(int index) {
  switch (int(index / ADC_PER_DEVICE)) {
    case 0:
      return max(adc_a->readADC_SingleEnded(index % ADC_PER_DEVICE), 0);
    case 1:
      return max(adc_b->readADC_SingleEnded(index % ADC_PER_DEVICE), 0);
  }
}

float ThermistorsADC::_adc_to_celsius(int _adc) {
  if (_adc < TABLE[ADC_TABLE_SIZE-1][0]) {
    return TABLE[ADC_TABLE_SIZE-1][1];
  }
  else if (_adc > TABLE[0][0]){
    return TABLE[0][1];
  }
  else {
    int ADC_LOW, ADC_HIGH;
    for (int i=0;i<ADC_TABLE_SIZE - 1;i++){
      ADC_HIGH = TABLE[i][0];
      ADC_LOW = TABLE[i+1][0];
      if (_adc >= ADC_LOW && _adc <= ADC_HIGH){
        float p = float(abs(ADC_HIGH - _adc)) / abs(ADC_HIGH - ADC_LOW);
        p *= float(abs(TABLE[i+1][1] - TABLE[i][1]));
        p += float(TABLE[i][1]);
        return p;
      }
    }
  }
}
