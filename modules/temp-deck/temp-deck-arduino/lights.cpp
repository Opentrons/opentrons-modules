#include "Lights.h"

Lights::Lights(){
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

bool Lights::_is_a_stable_number(int number) {
  if (number == _previous_display_number) {
    _same_display_number_count++;
    if (_same_display_number_count > _same_display_number_threshold) {
      return true;
    }
  }
  _previous_display_number = number;
  return false;
}

void Lights::_set_seven_segment(float * digit_1, float * digit_2) {
  for (uint8_t i=0;i<7;i++) {
    set_pwm_pin_inverse(segments_pin_mapping[0][i], digit_1[i] * numbers_brightness);
    set_pwm_pin_inverse(segments_pin_mapping[1][i], digit_2[i] * numbers_brightness);
  }
}

void Lights::display_number(int number, bool force=false) {
  if (force || _is_a_stable_number(number)) {
    _same_display_number_count = 0;  // reset the stable counter
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

void Lights::show_message_low(bool force=false){
  _set_seven_segment(message_low[0], message_low[1]);
}

void Lights::show_message_high(bool force=false){
  _set_seven_segment(message_high[0], message_high[1]);
}

void Lights::clear_display(){
  set_color_bar(0, 0, 0, 0);
  _set_seven_segment(seven_segment_blank, seven_segment_blank);
}

void Lights::set_color_bar(float red, float green, float blue, float white){
  analogWrite(red_led, red * 255.0 * color_bar_brightness);
  set_pwm_pin(green_pwm_pin, green * color_bar_brightness);
  analogWrite(blue_led, blue * 255.0 * color_bar_brightness);
  set_pwm_pin(white_pwm_pin, white * color_bar_brightness);
}

void Lights::set_color_bar_brightness(float brightness) {
  brightness = constrain(brightness, 0.0, 1.0);
  color_bar_brightness = brightness;
}

void Lights::set_numbers_brightness(float brightness) {
  brightness = constrain(brightness, 0.0, 1.0);
  numbers_brightness = brightness;
}

void Lights::startup_animation() {
  display_number(88, true);
  set_color_bar(0, 0, 0, 1);  // white (rgbw)
  delay(200);
  set_color_bar(0, 0, 1, 0);  // blue (rgbw)
  delay(200);
  set_color_bar(0, 1, 0, 0);  // green (rgbw)
  delay(200);
  set_color_bar(1, 0, 0, 0);  // red (rgbw)
  delay(200);
  set_color_bar(0, 0, 0, 1);  // end on white
}

void Lights::setup_lights() {
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  pwm.begin();
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency
  clear_display();
}
