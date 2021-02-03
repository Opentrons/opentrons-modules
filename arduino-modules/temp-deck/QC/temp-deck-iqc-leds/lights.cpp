#include "Lights.h"

Lights::Lights(){
/*
  we use Adafruit's 16-channel PWM I2C driver library
  can be found at:
  https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
*/
  pwm = Adafruit_PWMServoDriver();
}

void Lights::set_pwm_pin(int pin, float val) {
  val = (val * 256) * 16;
  val = constrain(val, 0, 4095);
  pwm.setPWM(pin, 0, val);
}

void Lights::set_pwm_pin_inverse(int pin, float val) {
  val = (val * 256) * 16;
  val = constrain(val, 0, 4095);
  pwm.setPWM(pin, val, 4095);
}

bool Lights::_is_a_stable_number(int number, bool debounce=true) {
  bool return_val = false;
  if (!debounce || (number == _previous_display_number && number != _previous_saved_number)) {
    _same_display_number_count++;
    if (!debounce || (_same_display_number_count > SAME_DISPLAY_NUMBER_THRESHOLD)) {
      _same_display_number_count = 0;  // reset the stable counter
      _previous_saved_number = number;
      return_val = true;
    }
  }
  _previous_display_number = number;
  return return_val;
}

void Lights::_set_seven_segment(bool * digit_1, bool * digit_2) {
  for (uint8_t i=0;i<NUM_SEGMENTS;i++) {
    delayMicroseconds(I2C_WRITE_DELAY_US);
    set_pwm_pin_inverse(segments_pin_mapping[0][i], float(digit_1[i]) * numbers_brightness);
    delayMicroseconds(I2C_WRITE_DELAY_US);
    set_pwm_pin_inverse(segments_pin_mapping[1][i], float(digit_2[i]) * numbers_brightness);
  }
}

void Lights::_update_flash_multiplier() {
  if (!is_flashing) {
    // only force it to be solid once the cycle is close to the top
    // if it is not at the top, then keep updating as if we are still flashing
    if (flash_multiplier < 1.0 && flash_multiplier > 0.9) {
      flash_multiplier = 1.0;
    }
    if (flash_multiplier == 1.0) {
      return;
    }
  }
  unsigned long now = millis();
  if (flash_timestamp > now) flash_timestamp = now; // handle rollover
  if (flash_timestamp + flash_interval < now) {
    flash_timestamp += flash_interval;
  }
  flash_multiplier = float(now - flash_timestamp) / float(flash_interval);
  flash_multiplier = (sin(flash_multiplier * PI * 2.0) / 2.0) + 0.5;
  flash_multiplier *= (1.0 - color_bar_min_brightness);
  flash_multiplier += color_bar_min_brightness;
}

void Lights::display_number(int number, bool debounce=true) {
  if (_is_a_stable_number(number, debounce)) {
    uint8_t single_digit_value = abs(number) % 10;
    if (number < 0) {
      _set_seven_segment(seven_segment_neg_symbol, numbers[single_digit_value]);
    }
    else if (number < 10) {
      _set_seven_segment(seven_segment_blank, numbers[single_digit_value]);
    }
    else {
      _set_seven_segment(numbers[abs(number) / 10], numbers[single_digit_value]);
    }
  }
}

void Lights::set_color_bar(float red, float green, float blue, float white){
  _update_flash_multiplier();
  _color_bar_current[0] = red * color_bar_brightness * flash_multiplier;
  _color_bar_current[1] = green * color_bar_brightness * flash_multiplier;
  _color_bar_current[2] = blue * color_bar_brightness * flash_multiplier;
  _color_bar_current[3] = white * color_bar_brightness * flash_multiplier;
  bool change = false;
  for (uint8_t i=0;i<4;i++){
    if (_color_bar_current[i] != _color_bar_previous[i]) {
      change = true;
    }
    _color_bar_previous[i] = float(_color_bar_current[i]);
  }
  if (change) {
    analogWrite(red_led, _color_bar_current[0] * 255.0);
    set_pwm_pin(green_pwm_pin, _color_bar_current[1]);
    analogWrite(blue_led, _color_bar_current[2] * 255.0);
    set_pwm_pin(white_pwm_pin, _color_bar_current[3]);
  }
}

void Lights::set_color_bar_brightness(float brightness) {
  brightness = constrain(brightness, 0.0, 1.0);
  color_bar_brightness = brightness;
}

void Lights::set_numbers_brightness(float brightness) {
  brightness = constrain(brightness, 0.0, 1.0);
  numbers_brightness = brightness;
}

void Lights::flash_on(int interval=1500) {
  is_flashing = true;
  flash_interval = interval;
}

void Lights::flash_off() {
  is_flashing = false;
}

void Lights::startup_animation(int target_number, int transition_time) {
  // the PWM driver starts off will both 7-segments displays at full brightness (aka 88)
  // and the color-bar all the way off

  // this animation transitions those states to the 'target_number' and color-bar at WHITE

  // store the previously-set brightnesses
  float target_color_bar_brightness = color_bar_brightness;
  float target_numbers_brightness = numbers_brightness;

  set_color_bar_brightness(0.0);
  set_color_bar(0, 0, 0, 1);
  set_numbers_brightness(1.0);
  display_number(88, false); // no debounce

  float scaler;
  // no need to handle rollover since this happens on boot
  unsigned long animation_start_time = millis();
  float half_time = transition_time / 2;

  // first, fade to black
  while (millis() < animation_start_time + half_time) {
    scaler = float(millis() - animation_start_time) / half_time;
    set_numbers_brightness(1.0 - scaler);
    display_number(88, false); // no debounce
  }

  set_color_bar_brightness(0.0);
  set_numbers_brightness(0.0);
  display_number(target_number, false); // no debounce

  // second, fade the number and color-bar up to the previously set brightnesses
  animation_start_time = millis();
  while (millis() < animation_start_time + half_time) {
    scaler = float(millis() - animation_start_time) / half_time;
    set_numbers_brightness(scaler * target_numbers_brightness);
    display_number(target_number, false); // no debounce
    set_color_bar_brightness(scaler * target_color_bar_brightness);
    set_color_bar(0, 0, 0, 1);
  }

  set_numbers_brightness(target_numbers_brightness);
  display_number(target_number, false); // no debounce
  set_color_bar_brightness(target_color_bar_brightness);
  set_color_bar(0, 0, 0, 1);
}

void Lights::setup_lights() {
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  Wire.setClock(400000);
  pwm.begin();
  delayMicroseconds(I2C_WRITE_DELAY_US);
  pwm.setPWMFreq(1600); // max frequency is 1600
  delayMicroseconds(I2C_WRITE_DELAY_US);
}
