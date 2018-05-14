#include "Thermistor.h"

Thermistor::Thermistor(){}

float Thermistor::_thermistor_temp_to_plate_temp(float thermistor_temp) {
  // The below linear function was found through using a measuring with a
  // thermocouple the temperature at the center of the plate, relative to
  // the temperature of this device's thermistor at the edge of the plate
  // TODO (andy) this should probably move to it's own function
  return (thermistor_temp * 0.937685) + 2.113056;
}

float Thermistor::_average_adc(int pin=-1, int numsamples=5) {
  uint8_t i;
  float samples[numsamples];
  float average;
  if (pin < 0){
    pin = thermistor_pin;
  }
  for (i=0; i< numsamples; i++) {
   samples[i] = analogRead(pin);
   delayMicroseconds(50);
  }
  average = 0;
  for (i=0; i< numsamples; i++) {
     average += samples[i];
  }
  return average /= numsamples;
}

float Thermistor::plate_temperature(float avg_adc=-1){
  if (avg_adc < 0) {
    avg_adc = _average_adc(thermistor_pin, 5);
  }
  if (avg_adc < TABLE[TABLE_SIZE-1][0]) {
    return _thermistor_temp_to_plate_temp(TABLE[TABLE_SIZE-1][1]);
  }
  else if (avg_adc > TABLE[0][0]){
    return _thermistor_temp_to_plate_temp(TABLE[0][1]);
  }
  else {
    int ADC_LOW, ADC_HIGH;
    for (int i=0;i<TABLE_SIZE - 1;i++){
      ADC_HIGH = TABLE[i][0];
      ADC_LOW = TABLE[i+1][0];
      if (avg_adc >= ADC_LOW && avg_adc <= ADC_HIGH){
        float adc_total_diff = abs(ADC_HIGH - ADC_LOW);
        float percent_from_colder = float(abs(ADC_HIGH - avg_adc)) / adc_total_diff;
        float temp_diff = float(abs(TABLE[i+1][1] - TABLE[i][1]));
        float thermistor_temp = float(TABLE[i][1]) + (percent_from_colder * temp_diff);
        return _thermistor_temp_to_plate_temp(thermistor_temp);
      }
    }
  }
}
