/********* INDICATOR NEOPIXELS *********/
#ifndef LIGHTS_H
#define LIGHTS_H

#include "Arduino.h"
#include <Adafruit_NeoPixel_ZeroDMA.h>

#define NEO_PWR     4
#define NEO_PIN     A5
#define NUM_PIXELS  16
#define WIPE_SPEED_DELAY 50
#define PULSE_UPDATE_INTERVAL 13 //millis

enum class TC_status
{
  idle,
  going_to_hot_target,
  going_to_cold_target,
  at_hot_target,
  at_cold_target
};

// Order is WRGB
// Brights are being toned down
#define COLOR_TABLE \
  COLOR_DEF(soft_white, 0xee000000), \
  COLOR_DEF(white, 0x00eeeeee),      \
  COLOR_DEF(red, 0x00500000),        \
  COLOR_DEF(green, 0x0000ee00),      \
  COLOR_DEF(blue, 0x000000ff),       \
  COLOR_DEF(orange, 0x00ff8300),     \
  COLOR_DEF(none, 0x00000000)

#define COLOR_DEF(color_name, _) color_name

enum class Light_action
{
  all_off=0,
  solid,
  pulsing,
  wipe
};

enum class Light_color
{
  COLOR_TABLE
};

class Lights
{
  public:
    Lights();
    void setup();
    void update();
    void set_lights(TC_status);
    void set_lights(Light_action, Light_color);
    Light_color api_color;
    Light_action api_action;
    bool color_override;
    bool action_override;
  private:
    Light_action _action, _prev_action;
    Light_color _color, _prev_color;
    void _set_strip_color(uint32_t color);
    void _color_wipe(Light_color color);
    void _set_pixel(int Pixel, Light_color color);
    void _pulse_leds(Light_color color);
};

#endif
