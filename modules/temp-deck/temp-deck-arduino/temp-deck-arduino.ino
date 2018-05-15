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
Gcode gcode = Gcode();

#define pin_tone_out 11
#define pin_fan_pwm 9

#define TEMPERATURE_MAX 99
#define TEMPERATURE_BURN 70
#define TEMPERATURE_MIN -9
#define TEMPERATURE_ROOM 25

int TARGET_TEMPERATURE = 25;
bool IS_TARGETING = false;

String device_serial = "TD001180622A01";
String device_model = "001";
String device_version = "edge-1a2b3c4";

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
}

void hot(float amount) {
  peltiers.set_hot_percentage(amount);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_target_temperature(int current_temp){
  peltiers.update_peltier_cycle();
  if (IS_TARGETING) {
    // if we've arrived, just be calm, but don't turn off
    if (current_temp == TARGET_TEMPERATURE) {
      if (TARGET_TEMPERATURE > 25.0) hot(0.5);
      else cold(1.0);
    }
    else if (TARGET_TEMPERATURE - current_temp > 0.0) {
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

void _set_color_bar_from_range(int val, int min, int middle, int max) {
  if (IS_TARGETING) {
    lights.set_color_bar_brightness(1.0);
  }
  else {
    lights.set_color_bar_brightness(0.1);
  }
  float cold[4] = {0, 0, 1, 0};
  float room[4] = {0, 0, 0, 1};
  float hot[4] = {1, 0, 0, 0};
  if (val == middle) {
    lights.set_color_bar(room[0], room[1], room[2], room[3]);
  }
  // cold
  else if (val < middle) {
    if (val < min) {
      lights.set_color_bar(cold[0], cold[1], cold[2], cold[3]);
    }
    else {
      // scale it to cold color
      float m = float(val - min) / float(middle - min);  // 1=room, 0=cold
      float r = (m * room[0]) + ((1.0 - m) * cold[0]);
      float g = (m * room[1]) + ((1.0 - m) * cold[1]);
      float b = (m * room[2]) + ((1.0 - m) * cold[2]);
      float w = (m * room[3]) + ((1.0 - m) * cold[3]);
      lights.set_color_bar(r, g, b, w);
    }
  }
  // hot
  else {
    if (val > max) {
      lights.set_color_bar(hot[0], hot[1], hot[2], hot[3]);
    }
    else {
      // scale it to hot color
      float m = float(val - middle) / float(max - middle);  // 1=hot, 0=room
      float r = (m * hot[0]) + ((1.0 - m) * room[0]);
      float g = (m * hot[1]) + ((1.0 - m) * room[1]);
      float b = (m * hot[2]) + ((1.0 - m) * room[2]);
      float w = (m * hot[3]) + ((1.0 - m) * room[3]);
      lights.set_color_bar(r, g, b, w);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_temperature_display(int current_temp, boolean force=false){
  lights.display_number(current_temp, force);
  _set_color_bar_from_range(
    current_temp, TEMPERATURE_MIN, TEMPERATURE_ROOM, TEMPERATURE_BURN);
  if (current_temp > TEMPERATURE_MAX || current_temp < TEMPERATURE_MIN){
    // flash the lights or something...
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
  disable_target();
  set_fan_percentage(0.0);
}

void print_temperature() {
  if (IS_TARGETING) {
    gcode.print_targetting_temperature(
      TARGET_TEMPERATURE,
      thermistor.plate_temperature()
    );
  }
  else {
    gcode.print_stablizing_temperature(
      thermistor.plate_temperature()
    );
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void start_dfu_timeout() {
  gcode.print_warning("Restarting and entering bootloader in 2 second...");
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_gcode(){
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_NO_CODE:
          break;
        case GCODE_GET_TEMP:
          print_temperature();
          break;
        case GCODE_SET_TEMP:
          if (gcode.read_int('S')) {
            set_target_temperature(gcode.parsed_int);
          }
          break;
        case GCODE_DISENGAGE:
          disengage();
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
  gcode.setup(115200);
  peltiers.setup_peltiers();
  set_fan_percentage(0);
  lights.setup_lights();
  disengage();
  lights.startup_animation();
  update_temperature_display(thermistor.plate_temperature(), true);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop(){
  read_gcode();
  int current_temp = thermistor.plate_temperature();
  update_temperature_display(current_temp);
  update_target_temperature(current_temp);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
