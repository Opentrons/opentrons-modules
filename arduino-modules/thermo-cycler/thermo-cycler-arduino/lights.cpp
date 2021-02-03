#include "lights.h"

#if RGBW_NEO
Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_GRBW);
#else
Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);
#endif

#undef COLOR_DEF
#define COLOR_DEF(_, color) color
#define COLOR_INDEX(color_name) static_cast<int>(color_name)

const uint32_t COLOR_CODES[] = { COLOR_TABLE };

Lights::Lights(){}

void Lights::setup()
{
  pinMode(NEO_PWR, OUTPUT);
  pinMode(NEO_PIN, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  strip.begin();
  strip.setBrightness(70);
  _action = Light_action::solid;
  _color = Light_color::white;
  strip.show();
}

void Lights::update()
{
  // Updates lights acc to _action and _color
  switch(_action)
  {
    case Light_action::solid:
      _set_strip_color(COLOR_CODES[COLOR_INDEX(_color)]);
      break;
    case Light_action::pulsing:
      _pulse_leds(_color);
      break;
    case Light_action::wipe:
      _color_wipe(_color);
      break;
    case Light_action::flashing:
      _flash_on_off(_color);
      break;
    case Light_action::all_off:
    default:
      _set_strip_color(COLOR_CODES[COLOR_INDEX(Light_color::none)]);
      break;
  }
  _prev_action = _action;
  _prev_color = _color;
}

void Lights::set_lights(TC_status tc_status)
{
  // Set _action and _color acc to tc_status
  switch(tc_status)
  {
    case TC_status::idle:
      _action = (action_override ? api_action : Light_action::solid);
      _color = (color_override ? api_color : Light_color::white);
      break;
    case TC_status::errored:
      _action = Light_action::flashing;
      _color = Light_color::orange;
      break;
    case TC_status::going_to_hot_target:
      _action = (action_override ? api_action : Light_action::pulsing);
      _color = (color_override ? api_color : Light_color::red);
      break;
    case TC_status::going_to_cold_target:
      _action = (action_override ? api_action : Light_action::pulsing);
      _color = (color_override ? api_color : Light_color::blue);
      break;
    case TC_status::at_hot_target:
      _action = (action_override ? api_action : Light_action::solid);
      _color = (color_override ? api_color : Light_color::red);
      break;
    case TC_status::at_cold_target:
      _action = (action_override ? api_action : Light_action::solid);
      _color = (color_override ? api_color : Light_color::blue);
      break;
    default:
      _action = Light_action::solid;
      _color = Light_color::orange;
      break;
  }
}

void Lights::set_lights(Light_action action, Light_color color)
{
  // Set _action and _color to args values
  _action = action;
  _color = color;
}

void Lights::_set_strip_color(uint32_t color)
{
  for(int i=0; i<strip.numPixels(); i++)
  {
    strip.setPixelColor(i, color); // strip.setPixelColor(n, [white, red, green, blue]);
    strip.show();
    delayMicroseconds(10);
  }
}

/* Lights::_color_wipe:
 * This function turns on each pixel in the strip incrementally after a small
 * delay, creating a 'wipe' of color. Once the entire strip is on, it
 * similarly turns each pixel off, making it look like the color is being
 * pushed off the other end of the strip. This creates the desired
 * continuously rotating light effect.
 */
void Lights::_color_wipe(Light_color color) {
  static uint8_t i = 0;
  static bool is_no_color_wipe = true;
  if (_prev_action != Light_action::wipe || _prev_color != color)
  {
    _set_strip_color(COLOR_CODES[COLOR_INDEX(color)]);
    i = 0;
    is_no_color_wipe = true;
  }
  static unsigned long last_update_millis = 0;
  if (millis() - last_update_millis < WIPE_SPEED_DELAY)
  {
    return;
  }
  if (i >= strip.numPixels())
  {
    i = 0;
    is_no_color_wipe = !is_no_color_wipe;
  }
  if (is_no_color_wipe)
  {
    _set_pixel(i, Light_color::none);
  }
  else
  {
    _set_pixel(i, color);
  }
  strip.show();
  delayMicroseconds(50);
  last_update_millis = millis();
  i++;
}

void Lights::_set_pixel(int pixel, Light_color color) {
   // NeoPixel
   strip.setPixelColor(pixel, COLOR_CODES[COLOR_INDEX(color)]);
}

void Lights::_pulse_leds(Light_color color)
{
  static float brightness = 0;
  static float rad = 0;
  static unsigned long last_update = 0;
  if (millis() - last_update >= PULSE_UPDATE_INTERVAL)
  {
    if (rad >= 3.14)
    {
      rad = 0;
    }

    uint32_t color32 = COLOR_CODES[COLOR_INDEX(color)];
    uint8_t w = ((color32 & 0xff000000) >> 24);
    uint8_t r = ((color32 & 0x00ff0000) >> 16);
    uint8_t g = ((color32 & 0x0000ff00) >> 8);
    uint8_t b = ((color32 & 0x000000ff) >> 0);
    if (r)
    {
      brightness = sin(rad) * r;
      r = uint8_t(brightness);
    }
    if (g)
    {
      brightness = sin(rad) * g;
      g = uint8_t(brightness);
    }
    if (b)
    {
      brightness = sin(rad) * b;
      b = uint8_t(brightness);
    }
    if (w)
    {
      brightness = sin(rad) * w;
      w = uint8_t(brightness);
    }

    uint32_t new_shade = (uint32_t(w) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | (uint32_t(b) << 0);
    _set_strip_color(new_shade);
    delayMicroseconds(10);
    last_update = millis();
    if (rad > 1.57)
    {
      rad += 0.06;
    }
    else
    {
      rad += 0.04;
    }
  }
}

void Lights::_flash_on_off(Light_color color)
{
  static unsigned long last_update = 0;
  static bool led_toggle_state = 0;
  if (millis() - last_update < FLASHING_INTERVAL)
  {
    return;
  }
  led_toggle_state ?
    _set_strip_color(COLOR_CODES[COLOR_INDEX(_color)]) :
    _set_strip_color(COLOR_CODES[COLOR_INDEX(Light_color::none)]);
  led_toggle_state = !led_toggle_state;
  last_update = millis();
}
