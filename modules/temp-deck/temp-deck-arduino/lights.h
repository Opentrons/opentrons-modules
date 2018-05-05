#ifndef Lights_h
#define Lights_h

#include "Arduino.h"

#include <Adafruit_PWMServoDriver.h>

#define red_led 5
#define blue_led 6

#define green_pwm_pin 0
#define white_pwm_pin 7

class Lights{
  public:

    Lights();
    void setup_lights();
    void startup_color_animation();
    void display_number(int number);
    void clear_display();
    void set_color_bar(float red, float green, float blue, float white);

  private:

    Adafruit_PWMServoDriver pwm;
    const uint8_t digit_pin_mapping[2][7] = {
      {10, 11, 4, 5, 6, 9, 8},
      {14, 15, 1, 2, 3, 13, 12}
    };
    const uint8_t numbers[10][7] = {
      {1, 1, 1, 1, 1, 1, 0},  // 0
      {0, 1, 1, 0, 0, 0, 0},  // 1
      {1, 1, 0, 1, 1, 0, 1},  // 2
      {1, 1, 1, 1, 0, 0, 1},  // 3
      {0, 1, 1, 0, 0, 1, 1},  // 4
      {1, 0, 1, 1, 0, 1, 1},  // 5
      {1, 0, 1, 1, 1, 1, 1},  // 6
      {1, 1, 1, 0, 0, 0, 0},  // 7
      {1, 1, 1, 1, 1, 1, 1},  // 8
      {1, 1, 1, 1, 0, 1, 1}   // 9
    };

    const uint8_t message_low[2][7] = {
      {0, 0, 0, 1, 1, 1, 0},  // L
      {1, 1, 1, 1, 1, 1, 0}   // O
    };

    const uint8_t message_high[2][7] = {
      {0, 1, 1, 0, 1, 1, 1},  // H
      {0, 1, 1, 0, 0, 0, 0}   // I
    };

    void set_pwm_pin(int pin, float val);
    void show_message_low();
    void show_message_high();
};

#endif
