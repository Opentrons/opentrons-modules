#include "Lights.h"

Lights::Lights(){
  pwm = Adafruit_PWMServoDriver();
}

void Lights::set_pwm_pin(int pin, float val) {
  val = (val * 256) * 16;
  pwm.setPWM(pin, 4096 - val, val);
}

bool Lights::_is_a_stable_number(int number) {
  if (number == _previous_display_number) {
    _same_display_number_count++;
    if (_same_display_number_count > _same_display_number_threshold) {
      return true;
    }
  }
  return false;
}

void Lights::_set_seven_segment(uint8_t digit_1[], uint8_t digit_2[]) {
  for (uint8_t i=0;i<7;i++) {
    set_pwm_pin(segments_pin_mapping[0][i], digit_1[i]);
    set_pwm_pin(segments_pin_mapping[1][i], digit_2[i]);
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
      _set_seven_segment(abs(number) / 10, numbers[single_digit_value]);
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
  _set_seven_segment(seven_segment_blank, seven_segment_blank);
}

void Lights::set_color_bar(float red, float green, float blue, float white){
  green = 1.0 - green;
  white = 1.0 - white;
  analogWrite(red_led, red * 255);
  set_pwm_pin(green_pwm_pin, green);
  analogWrite(blue_led, blue * 255);
  set_pwm_pin(white_pwm_pin, white);
}

void Lights::startup_color_animation() {
  display_number(88);
  set_color_bar(0, 0, 0, 1);  // white (rgbw)
  delay(200);
  set_color_bar(0, 0, 1, 0);  // blue (rgbw)
  delay(200);
  set_color_bar(0, 1, 0, 0);  // green (rgbw)
  delay(200);
  set_color_bar(1, 0, 0, 0);  // red (rgbw)
  delay(200);
  set_color_bar(0, 0, 0, 1);
}

void Lights::setup_lights() {
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  pwm.begin();
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency
  clear_display();
}
