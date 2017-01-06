/*
  To set the target temperature (celsius), change the value below.
  For example, if I wanted 45 degrees C, I would set:

    const double target_temperature = 45;

  After editing the value to your desired temperature, connect
  the heat deck over USB, and upload from the Arduino IDE.
  Make sure to set the "Tools > Boards" setting to "Uno"

  You can see a printout of the currently read temperature
  by opening the Serial Monitor at baudrate 115200

  This sketch was created by Adafruit:
  https://learn.adafruit.com/thermistor/using-a-thermistor
  and modified by Opentrons to work with the Heat Deck.
*/



const double target_temperature = 50;




// which analog pin to connect
#define THERMISTORPIN A0         
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
double BCOEFFICIENT = 3250;
// the value of the 'other' resistor
#define SERIESRESISTOR 4700    
 
int samples[NUMSAMPLES];
 
void setup(void) {
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
}
 
void loop(void) {
  uint8_t i;
  float average;
 
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = 1023 - analogRead(THERMISTORPIN);
   delay(10);
  }
 
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;
 
//  Serial.print("Average analog reading "); 
//  Serial.println(average);
 
  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

//  Serial.print("Thermistor resistance "); 
//  Serial.println(average);
 
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  // scaling to get better accuracy from Heat Deck
  steinhart = ((steinhart - TEMPERATURENOMINAL) * 1.05) + TEMPERATURENOMINAL;
 
  Serial.print("Temperature "); 
  Serial.print(steinhart);
  Serial.println(" *C");

  if (steinhart < target_temperature){
    digitalWrite(10, LOW);
  }
  else {
    digitalWrite(10, HIGH);
  }
 
  delay(200);
}
