/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Arduino.h>
#include "lights.h"
#include "peltiers.h"
#include "thermistor.h"

Lights lights = Lights();
Peltiers peltiers = Peltiers();
Thermistor thermistor = Thermistor();

#define tone_out 11

#define fan_pwm 9

int prev_temp = 0;

int TARGET_TEMPERATURE = 25;
boolean IS_TARGETING = false;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_target_temperature(int target_temp){
  peltiers.disable_peltiers();
  disable_target();
  if (target_temp < 0) target_temp = 0;
  if (target_temp > 99) target_temp = 99;
  IS_TARGETING = true;
  TARGET_TEMPERATURE = target_temp;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disable_target(){
  IS_TARGETING = false;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void delay_minutes(int minutes){
  for (int i=0;i<minutes;i++){
    for (int s=0; s<60; s++){
      delay(1000);  // 1 minute
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_percentage(float percentage){
  if (percentage < 0) percentage = 0;
  if (percentage > 1) percentage = 1;
  analogWrite(fan_pwm, int(percentage * 255.0));
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void cold(float amount) {
  peltiers.set_cold_percentage(amount);
  lights.set_color_bar(0, 0, 1, 0);
}

void hot(float amount) {
  peltiers.set_hot_percentage(amount);
  lights.set_color_bar(1, 0, 0, 0);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_target_temperature(boolean set_fan=true){
  peltiers.update_peltier_cycle();
  if (IS_TARGETING) {
    float temp = thermistor.peltier_temperature();
    // HOLD TEMP
    if (int(temp) == TARGET_TEMPERATURE) {
      lights.set_color_bar(0, 1, 0, 0);
      if (TARGET_TEMPERATURE > 25.0) {
        hot(0.5);
        if (set_fan) set_fan_percentage(0.5);
      }
      else {
        cold(1.0);
        if (set_fan) set_fan_percentage(1.0);
      }
    }
    // HEAT UP
    else if (TARGET_TEMPERATURE - int(temp) > 0.0) {
      if (TARGET_TEMPERATURE > 25.0) {
        hot(1.0);
        if (set_fan) set_fan_percentage(0.5);
      }
      else {
        hot(0.2);
        if (set_fan) set_fan_percentage(1.0);
      }
    }
    // COOL DOWN
    else {
      if (TARGET_TEMPERATURE < 25.0) {
        cold(1.0);
        if (set_fan) set_fan_percentage(1.0);
      }
      else {
        cold(0.2);
        if (set_fan) set_fan_percentage(0.5);
      }
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_temperature_display(boolean force=false){
  int current_temp = thermistor.peltier_temperature();
  boolean stable = true;
//  for (int i=0;i<10;i++){
//    if (int(thermistor.peltier_temperature()) != current_temp) {
//      stable = false;
//      break;
//    }
//  }
  if (stable || force) {
    if (current_temp != prev_temp || force){
      lights.display_number(current_temp);
    }
    prev_temp = current_temp;
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

boolean DOING_CYCLE_TEST = false;

const int cycle_high = 90;
const int cycle_low = 5;

void cycle_test(){
  set_fan_percentage(1.0);
  if (TARGET_TEMPERATURE == int(thermistor.peltier_temperature())){
    if (TARGET_TEMPERATURE == cycle_high){
      set_target_temperature(cycle_low);
    }
    else {
      set_target_temperature(cycle_high);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void activate_bootloader(){
  // Need to verify if a reset to application code 
  // is performed properly after DFU
  cli();
  // disable watchdog, if enabled
  // disable all peripherals
  UDCON = 1;
  USBCON = (1<<FRZCLK);  // disable USB
  UCSR1B = 0;
  _delay_ms(5);
  #if defined(__AVR_ATmega32U4__)              
      EIMSK = 0; PCICR = 0; SPCR = 0; ACSR = 0; EECR = 0; ADCSRA = 0;
      TIMSK0 = 0; TIMSK1 = 0; TIMSK3 = 0; TIMSK4 = 0; UCSR1B = 0; TWCR = 0;
      DDRB = 0; DDRC = 0; DDRD = 0; DDRE = 0; DDRF = 0; TWCR = 0;
      PORTB = 0; PORTC = 0; PORTD = 0; PORTE = 0; PORTF = 0;
      asm volatile("jmp 0x3800");   //Bootloader start address
  #endif 
}
/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_serial(){
  if (Serial.available()){
    char letter = Serial.read();
    int val;
    if (letter == 'c'){
      val = Serial.parseInt();
      cold(float(val) / 100.0);
      disable_target();
    }
    else if (letter == 'h'){
      val = Serial.parseInt();
      hot(float(val) / 100.0);
      disable_target();
    }
    else if (letter == 'x'){
      DOING_CYCLE_TEST = false;
      peltiers.disable_peltiers();
      lights.set_color_bar(0, 0, 0, 1);
      disable_target();
      set_fan_percentage(0.0);
    }
    else if (letter == 'f'){
      val = Serial.parseInt();
      set_fan_percentage(float(val) / 100.0);
    }
    else if (letter == 't'){
      val = Serial.parseInt();
      set_fan_percentage(0.0);
      unsigned long now = millis();
      while (now + 2500 > millis()){  // wait for fan to shutdown
        update_temperature_display();
      }
      set_target_temperature(val);
      now = millis();
      while (now + 2500 > millis()){  // engage peltiers, and let current settle
        update_target_temperature(false);
        update_temperature_display();
      }
    }
    else if (letter == 'p') {
      Serial.print(thermistor.average_adc());
      Serial.print("\t");
      Serial.print(thermistor.peltier_temperature());
      Serial.println(" deg-C");
    }
    else if (letter == 'l'){
      lights.startup_color_animation();
    }
    else if (letter == 'r') {
      DOING_CYCLE_TEST = true;
      set_target_temperature(cycle_low);
    }
    else if (letter == 'u') {
      //Update firmware
      //TODO Send proper ACK
      Serial.println("Entering Bootloader");
      delay(2000);
      activate_bootloader();
    }
    while (Serial.available() > 0) {
      Serial.read();
    }
    Serial.println("ok");
    Serial.println("ok");
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {
  pinMode(tone_out, OUTPUT);
  pinMode(fan_pwm, OUTPUT);

  peltiers.setup_peltiers();
  set_fan_percentage(0);
  lights.setup_lights();
  lights.startup_color_animation();
  lights.set_color_bar(0, 0, 0, 1);
  update_temperature_display(true);

  Serial.begin(115200);
  Serial.setTimeout(5);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop(){
  read_serial();
  update_temperature_display();
  update_target_temperature();
  if (DOING_CYCLE_TEST){
    cycle_test();
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
