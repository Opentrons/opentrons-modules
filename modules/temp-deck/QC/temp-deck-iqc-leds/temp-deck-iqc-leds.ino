/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Arduino.h>

#include "lights.h"


Lights lights = Lights();  // controls 2-digit 7-segment numbers, and the RGBW color bar

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {
  lights.setup_lights();
  lights.set_numbers_brightness(0.25);
  lights.set_color_bar_brightness(0.5);
  lights.flash_off();
}

void loop(){
  lights.set_color_bar(0, 0, 0, 1);  // white
  for (uint8_t i=0;i<9;i++){
    lights.display_number((i + 1) * 11, false);
    delay(200);
  }
  lights.display_number(88, false);
  lights.set_color_bar(0, 0, 1, 0);  // blue
  delay(500);
  lights.display_number(88, false);
  lights.set_color_bar(0, 1, 0, 0);  // green
  delay(500);
  lights.display_number(88, false);
  lights.set_color_bar(1, 0, 0, 0);  // red
  delay(500);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
