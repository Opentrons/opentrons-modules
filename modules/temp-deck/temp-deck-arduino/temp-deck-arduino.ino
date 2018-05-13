/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Arduino.h>
#include "lights.h"
#include "peltiers.h"
#include "thermistor.h"
#include "gcode.h"

Lights lights = Lights();
Peltiers peltiers = Peltiers();
Thermistor thermistor = Thermistor();
Gcode gcode = Gcode(115200);

#define pin_tone_out 11
#define pin_fan_pwm 9

#define TEMPERATURE_MAX 99
#define TEMPERATURE_MIN -9

int prev_temp = 0;

int TARGET_TEMPERATURE = 25;
boolean IS_TARGETING = false;

String device_serial = "TD001180622A01"
String device_model = "001"
String device_version = "edge-1a2b3c4"

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_target_temperature(int target_temp){
  if (target_temp < TEMPERATURE_MIN) {
    target_temp = TEMPERATURE_MIN;
    gcode.print_warning(
      "Target temperature too low, setting to TEMPERATURE_MIN degrees");
  }
  if (target_temp > TEMPERATURE_MAX) {
    target_temp = TEMPERATURE_MAX;
    gcode.print_warning(
      "Target temperature too high, setting to TEMPERATURE_MAX degrees");
  }
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
  analogWrite(pin_fan_pwm, int(percentage * 255.0));
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

void update_target_temperature(){
  peltiers.update_peltier_cycle();
  if (IS_TARGETING) {
    float temp = thermistor.plate_temperature();
    // if we've arrived, just be calm, but don't turn off
    if (int(temp) == TARGET_TEMPERATURE) {
      lights.set_color_bar(0, 1, 0, 0);
      if (TARGET_TEMPERATURE > 25.0) hot(0.5);
      else cold(1.0);
    }
    else if (TARGET_TEMPERATURE - int(temp) > 0.0) {
      if (TARGET_TEMPERATURE > 25.0) hot(1.0);
      else hot(0.2);
    }
    else {
      if (TARGET_TEMPERATURE < 25.0) cold(1.0);
      else cold(0.2);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_temperature_display(boolean force=false){
  int current_temp = thermistor.plate_temperature();
  boolean stable = true;
//  for (int i=0;i<10;i++){
//    if (int(thermistor.plate_temperature()) != current_temp) {
//      stable = false;
//      break;
//    }
//  }
  if (stable || force) {
    if (current_temp != prev_temp || force){
      if (number > 99){
        lights.show_message_low();
      }
      else if (number < 0){
        lights.show_message_high();
      }
      else {
        lights.display_number(current_temp);
      }
    }
    prev_temp = current_temp;
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

void disengage() {
  peltiers.disable_peltiers();
  lights.set_color_bar(0, 0, 0, 1);
  disable_target();
  set_fan_percentage(0.0);
}

void print_temperature() {
  if (IS_TARGETING) {
    gcode.print_stablizing_temperature(
      thermistor.plate_temperature()
    );
  }
  else {
    gcode.print_targetting_temperature(
      TARGET_TEMPERATURE,
      thermistor.plate_temperature()
    );
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_gcode(){
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_GET_TEMP:
          print_temperature();
          break;
        case GCODE_SET_TEMP:
          if (gcode.parse_int('S')) {
            set_target_temperature(gcode.parsed_int);
          }
          break;
        case GCODE_DISENGAGE:
          disengage()
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, device_version);
          break;
        case GCODE_DFU:
          start_dfu_timeout();
          break;
        default:
          break;
      }
    }
    gcode.send_ack();
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {
  pinMode(pin_tone_out, OUTPUT);
  pinMode(pin_fan_pwm, OUTPUT);

  gcode.setup();
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
  read_gcode();
  update_temperature_display();
  update_target_temperature();
  if (DOING_CYCLE_TEST){
    cycle_test();
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
