#include "Thermistor.h"

Thermistor::Thermistor(){}

float Thermistor::average_adc(int pin=-1, int numsamples=5) {
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

float Thermistor::peltier_temperature(float avg_adc=-1){
  if (avg_adc < 0) {
    avg_adc = average_adc(thermistor_pin, 5);
  }
  if (avg_adc < TABLE[TABLE_SIZE-1][0]) {
    return -1;
  }
  else if (avg_adc > TABLE[0][0]){
    return 100;
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
        float thermister_temp = float(TABLE[i][1]) + (percent_from_colder * temp_diff);

        // then convert
        return (thermister_temp * 0.937685) + 2.113056;
      }
    }
  }
}
