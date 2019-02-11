#include "ThermistorsADC.h"

ThermistorsADC::ThermistorsADC() {}

float ThermistorsADC::temperature(int index){
  int _adc = _read_adc(index);
  return _adc_to_celsius(_adc);
}

void ThermistorsADC::begin() {
  adc_a = new Adafruit_ADS1115(ADDRESS_A);
  adc_b = new Adafruit_ADS1115(ADDRESS_B);
  adc_c = new Adafruit_ADS1115(ADDRESS_C);
  adc_d = new Adafruit_ADS1115(ADDRESS_D);

  adsGain_t gain = gain_settings[0];  // safest gain by default
  for (uint8_t i=1;i<TOTAL_GAIN_SETTINGS;i++) {
    if (voltage < gain_max_voltage[i]) {
      gain = gain_settings[i];
    }
  }

  adc_a->setGain(gain);
  adc_b->setGain(gain);
  adc_c->setGain(gain);
  adc_d->setGain(gain);

  adc_a->begin();
  adc_b->begin();
  adc_c->begin();
  adc_d->begin();
}

void ThermistorsADC::begin(float _voltage) {
  voltage = _voltage;
  begin();
}

int ThermistorsADC::_read_adc(int index) {
  delay(1);
  if (index < 4) {
    return adc_a->readADC_SingleEnded(index);
  }
  else if (index < 8){
    return adc_b->readADC_SingleEnded(index - 4);
  }
  else if (index < 12){
    return adc_c->readADC_SingleEnded(index - 8);
  }
  else if (index < 16){
    return adc_d->readADC_SingleEnded(index - 12);
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
        float percent_from_colder = float(abs(ADC_HIGH - _adc)) / abs(ADC_HIGH - ADC_LOW);
        return float(TABLE[i][1]) + (percent_from_colder * float(abs(TABLE[i+1][1] - TABLE[i][1])));
      }
    }
  }
}
