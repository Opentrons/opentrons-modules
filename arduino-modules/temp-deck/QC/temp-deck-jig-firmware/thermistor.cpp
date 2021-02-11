#include "Thermistor.h"

Thermistor::Thermistor(){}

void Thermistor::_calculate_average_adc() {
  _average_adc = 0;
  for (uint8_t i=0; i< THERMISTOR_NUM_SAMPLES; i++) {
     _average_adc += samples[i];
  }
  _average_adc /= THERMISTOR_NUM_SAMPLES;
}

float Thermistor::temperature(){
  _calculate_average_adc();
  if (_average_adc < TABLE[TABLE_SIZE-1][0]) {
    return TABLE[TABLE_SIZE-1][1];
  }
  else if (_average_adc > TABLE[0][0]){
    return TABLE[0][1];
  }
  else {
    int ADC_LOW, ADC_HIGH;
    for (int i=0;i<TABLE_SIZE - 1;i++){
      ADC_HIGH = TABLE[i][0];
      ADC_LOW = TABLE[i+1][0];
      if (_average_adc >= ADC_LOW && _average_adc <= ADC_HIGH){
        float percent_from_colder = float(abs(ADC_HIGH - _average_adc)) / abs(ADC_HIGH - ADC_LOW);
        return float(TABLE[i][1]) + (percent_from_colder * float(abs(TABLE[i+1][1] - TABLE[i][1])));
      }
    }
  }
}

bool Thermistor::update() {
  samples[sample_index] = analogRead(thermistor_pin);
  sample_index++;
  if (sample_index >= THERMISTOR_NUM_SAMPLES) {
    sample_index = 0;
    return true;
  }
  return false;
}
