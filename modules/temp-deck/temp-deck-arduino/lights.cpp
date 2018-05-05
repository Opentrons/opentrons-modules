#include "Lights.h"

Lights::Lights(){
  pwm = Adafruit_PWMServoDriver();
}

void Lights::set_pwm_pin(int pin, float val) {
  val = (val * 256) * 16;
  pwm.setPWM(pin, 4096 - val, val);
}

void Lights::display_number(int number) {
  if (number > 99){
    show_message_low();
    return;
  }
  else if (number < 0){
    show_message_high();
    return;
  }
  uint8_t first_character = number / 10;
  uint8_t second_character = number % 10;
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(digit_pin_mapping[0][i], numbers[first_character][i]);
    set_pwm_pin(digit_pin_mapping[1][i], numbers[second_character][i]);
  }
}

void Lights::show_message_low(){
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(digit_pin_mapping[0][i], message_low[0][i]);
    set_pwm_pin(digit_pin_mapping[1][i], message_low[1][i]);
  }
}

void Lights::show_message_high(){
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(digit_pin_mapping[0][i], message_high[0][i]);
    set_pwm_pin(digit_pin_mapping[1][i], message_high[1][i]);
  }
}

void Lights::clear_display(){
  for (uint8_t l=0; l < 7; l++) {
    set_pwm_pin(digit_pin_mapping[0][l], 0);
    set_pwm_pin(digit_pin_mapping[1][l], 0);
  }
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
