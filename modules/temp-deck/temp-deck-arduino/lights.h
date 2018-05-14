// lights.h
// Controls an Indicator RGBW LED and two 7-segment displays
// Uses Adafruit PWM Servo Driver to drive the two displays as well as the green & white LED pins

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
    void display_number(int number, bool force=false);
    void clear_display();
    void set_color_bar(float red, float green, float blue, float white);
    void show_message_low(bool force=false);
    void show_message_high(bool force=false);

  private:

    Adafruit_PWMServoDriver pwm;
    const uint8_t segments_pin_mapping[2][7] = {
      {10, 11, 4, 5, 6, 9, 8},
      {14, 15, 1, 2, 3, 13, 12}
    };

    const uint8_t seven_segment_blank = {0, 0, 0, 0, 0, 0, 0};
    const uint8_t seven_segment_on = {1, 1, 1, 1, 1, 1, 1};
    const uint8_t seven_segment_neg_symbol = {0, 0, 0, 0, 0, 0, 1};

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

    int _same_display_number_count = 0;
    int _previous_display_number = -100;
    const int _same_display_number_threshold = 100;

    void set_pwm_pin(int pin, float val);
    bool _is_a_stable_number(int number);
    void _set_seven_segment(int digit_1[], int digit_2[]);
};

#endif
