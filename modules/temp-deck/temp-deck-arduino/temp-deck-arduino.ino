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
#define TEMPERATURE_MIN -9

#define TEMPERATURE_FEELS_COLD 10
#define TEMPERATURE_ROOM 25
#define TEMPERATURE_FEELS_HOT 60

bool START_BOOTLOADER = false;
unsigned long start_bootloader_timestamp = 0;
const int start_bootloader_timeout = 1000;  // 3 seconds

unsigned long SET_TEMPERATURE_TIMESTAMP = 0;
const unsigned long millis_till_fan_turns_off = 2500;
const unsigned long millis_till_peltiers_drop_current = 2500;

int TARGET_TEMPERATURE = TEMPERATURE_ROOM;
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
  SET_TEMPERATURE_TIMESTAMP = millis();
  lights.flash_on();
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
  percentage = constrain(percentage, 0.0, 1.0);
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

void stabilize_to_target_temp(int current_temp, bool set_fan=false){
  peltiers.update_peltier_cycle();
  if (IS_TARGETING) {
    unsigned long end_time = SET_TEMPERATURE_TIMESTAMP + millis_till_fan_turns_off;
    if (end_time > millis()) {
      peltiers.disable_peltiers();
      set_fan_percentage(0.0);
      return;
    }
    end_time += millis_till_peltiers_drop_current;
    if (end_time > millis()) {
      set_fan = false;
      set_fan_percentage(0.0);
    }
    // if we've arrived, just be calm, but don't turn off
    if (current_temp == TARGET_TEMPERATURE) {
      if (TARGET_TEMPERATURE > TEMPERATURE_ROOM) {
        hot(0.5);
        if (set_fan) set_fan_percentage(0.5);
      }
      else {
        cold(1.0);
        if (set_fan) set_fan_percentage(1.0);
      }
    }
    else if (TARGET_TEMPERATURE - current_temp > 0.0) {
      if (TARGET_TEMPERATURE > TEMPERATURE_ROOM) {
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
      if (TARGET_TEMPERATURE < TEMPERATURE_ROOM) {
        cold(1.0);
        if (set_fan) set_fan_percentage(1.0);
      }
      else {
        cold(1.0);
        if (set_fan) set_fan_percentage(0.5);
      }
    }
  }
  else {
    // not targetting, so if we are far away from room temperature, put
    // the fan on at a lower power level
    if (abs(current_temp - TEMPERATURE_ROOM) > 5) {
      set_fan_percentage(0.5);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void _set_color_bar_from_range(int val, int middle) {
  /*
    This method uses a temperature range (Celsius), to set the color of the
    RGBW color bar. It uses three data points in Celsius (min, middle, max),
    and does a linear transition between them, then multiplies by the three
    corresponding colors (cold, room, hot), to create a fade between colors
  */
  float cold[4] = {0, 0, 1, 0};
  float room[4] = {0, 0, 0, 1};
  float hot[4] = {1, 0, 0, 0};
  if (!IS_TARGETING || abs(val - TARGET_TEMPERATURE) < 2) {
    lights.flash_off();
  }
  else {
    lights.flash_on();
  }
  if (TARGET_TEMPERATURE == middle || !IS_TARGETING) {
    lights.set_color_bar(room[0], room[1], room[2], room[3]);
  }
  // cold
  else if (TARGET_TEMPERATURE < middle) {
    lights.set_color_bar(cold[0], cold[1], cold[2], cold[3]);
  }
  // hot
  else {
    lights.set_color_bar(hot[0], hot[1], hot[2], hot[3]);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_temperature_display(int current_temp, boolean force=false){
  lights.display_number(current_temp, force);
  _set_color_bar_from_range(current_temp, TEMPERATURE_ROOM);
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
  IS_TARGETING = false;
  set_fan_percentage(0.0);
  lights.flash_off();
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
  gcode.print_warning("Restarting and entering bootloader in 1 second...");
  START_BOOTLOADER = true;
  start_bootloader_timestamp = millis();
}

void check_if_bootloader_starts() {
  if (START_BOOTLOADER) {
    if (start_bootloader_timestamp + start_bootloader_timeout < millis()) {
      activate_bootloader();
    }
  }
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
  lights.set_numbers_brightness(0.5);
  lights.set_color_bar_brightness(1.0);
  disengage();
  update_temperature_display(thermistor.plate_temperature(), true);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop(){
  read_gcode();
  int current_temp = thermistor.plate_temperature();
  update_temperature_display(current_temp, false);
  stabilize_to_target_temp(current_temp, true);
  check_if_bootloader_starts();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
