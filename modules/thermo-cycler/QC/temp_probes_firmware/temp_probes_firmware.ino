/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Adafruit_MAX31865.h>
// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 rtdSensor = Adafruit_MAX31865(10);

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  100.0

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include "thermistorsadc.h"

ThermistorsADC thermistors = ThermistorsADC();

#define TOTAL_THERMISTORS 2
#define THERMISTOR_VOLTAGE 1.5

double temperatures[TOTAL_THERMISTORS] = {0, };
double rtd_temp_sum = 0;
double sample_count = 0;

uint16_t rtd;
float ratio;
uint8_t fault;
float temp = 0;
float max = 0;
float min = 0;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

double read_rtd() {
  fault = rtdSensor.readFault();
  if (fault) {
    Serial.print(F("Fault 0x")); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println(F("RTD High Threshold")); 
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println(F("RTD Low Threshold")); 
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println(F("REFIN- > 0.85 x Bias")); 
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println(F("REFIN- < 0.85 x Bias - FORCE- open")); 
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println(F("RTDIN- < 0.85 x Bias - FORCE- open")); 
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println(F("Under/Over voltage"));
    }
    rtdSensor.clearFault();
  }
  else {
    rtd = rtdSensor.readRTD();
    ratio = rtd;
    ratio /= 32768;
    return rtdSensor.temperature(RNOMINAL, RREF);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

    Serial.begin(115200);
    Serial.setTimeout(5);
    thermistors.begin(THERMISTOR_VOLTAGE);
    rtdSensor.begin(MAX31865_2WIRE);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop() {
  delay(10);
  for (uint8_t i=0;i<TOTAL_THERMISTORS;i++) {
    temperatures[i] += thermistors.temperature(i);
  }
  rtd_temp_sum += read_rtd();
  sample_count += 1;
  if (Serial.available()) {
    temp = 0;
    max = -50;
    min = 2000;
    for (uint8_t i=0;i<TOTAL_THERMISTORS;i++) {
      temp = temperatures[i] / sample_count;
      temperatures[i] = 0;
      Serial.print(temp);
      Serial.print('\t');
      if (temp > max) max = temp;
      if (temp < min) min = temp;
    }
    Serial.print(rtd_temp_sum / sample_count);
    rtd_temp_sum = 0;
    Serial.println();Serial.println();
    sample_count = 0;
    while (Serial.available()) {
      Serial.read(); delay(2);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
