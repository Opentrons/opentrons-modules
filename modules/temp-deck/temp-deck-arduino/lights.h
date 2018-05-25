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
    void startup_animation();
    void display_number(int number, bool force=false);
    void clear_display();
    void set_color_bar(float red, float green, float blue, float white);
    void show_message_low(bool force=false);
    void show_message_high(bool force=false);
    void set_color_bar_brightness(float brightness);
    void set_numbers_brightness(float brightness);
    void flash_on(int interval=1000);
    void flash_off();

  private:

    Adafruit_PWMServoDriver pwm;
    const uint8_t segments_pin_mapping[2][7] = {
      {10, 11, 4, 5, 6, 9, 8},
      {14, 15, 1, 2, 3, 13, 12}
    };

    const float seven_segment_blank[7] = {0, 0, 0, 0, 0, 0, 0};
    const float seven_segment_on[7] = {1, 1, 1, 1, 1, 1, 1};
    const float seven_segment_neg_symbol[7] = {0, 0, 0, 0, 0, 0, 1};

    const float numbers[10][7] = {
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

    const float message_low[2][7] = {
      {0, 0, 0, 1, 1, 1, 0},  // L
      {1, 1, 1, 1, 1, 1, 0}   // O
    };

    const float message_high[2][7] = {
      {0, 1, 1, 0, 1, 1, 1},  // H
      {0, 1, 1, 0, 0, 0, 0}   // I
    };

    int _same_display_number_count = 0;
    int _previous_display_number = -100;
    const int _same_display_number_threshold = 100;

    float color_bar_brightness = 1.0;
    float numbers_brightness = 1.0;

    void set_pwm_pin(int pin, float val);
    void set_pwm_pin_inverse(int pin, float val);
    bool _is_a_stable_number(int number);
    void _set_seven_segment(float digit_1[], float digit_2[]);

    unsigned long flash_timestamp = 0;
    float flash_multiplier = 1.0;
    int flash_direction = -1;  // 1 is up, -1 is down
    bool is_flashing = false;
    int flash_interval = 1000;
    const float color_bar_min_brightness = 0.1;
};

#endif
