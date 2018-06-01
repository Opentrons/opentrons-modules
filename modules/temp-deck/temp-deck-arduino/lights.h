// lights.h
// Controls an Indicator RGBW LED and two 7-segment displays
// Uses Adafruit PWM Servo Driver to drive the two displays as well as the green & white LED pins

#ifndef Lights_h
#define Lights_h

#include "Arduino.h"

/*
  we use Adafruit's 16-channel PWM I2C driver library
  can be found at:
  https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
*/
#include <Adafruit_PWMServoDriver.h>

#define red_led 5
#define blue_led 6

#define NUM_DIGITS 2
#define NUM_SEGMENTS 7

#define green_pwm_pin 0
#define white_pwm_pin 7

#define I2C_WRITE_DELAY_US 20

#define DEFAULT_FLASH_INTERVAL 1500

class Lights{
  public:

    Lights();
    void setup_lights();
    void startup_animation(int target_number, int transition_time);
    void display_number(int number, bool debounce=true);
    void set_color_bar(float red, float green, float blue, float white);
    void set_color_bar_brightness(float brightness);
    void set_numbers_brightness(float brightness);
    void flash_on(int interval=DEFAULT_FLASH_INTERVAL);
    void flash_off();

  private:

    Adafruit_PWMServoDriver pwm;
    const uint8_t segments_pin_mapping[NUM_DIGITS][NUM_SEGMENTS] = {
      {10, 11, 4, 5, 6, 9, 8},
      {14, 15, 1, 2, 3, 13, 12}
    };

    const float seven_segment_blank[NUM_SEGMENTS] = {0, 0, 0, 0, 0, 0, 0};
    const float seven_segment_on[NUM_SEGMENTS] = {1, 1, 1, 1, 1, 1, 1};
    const float seven_segment_neg_symbol[NUM_SEGMENTS] = {0, 0, 0, 0, 0, 0, 1};

    const float numbers[10][NUM_SEGMENTS] = {
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

    int _same_display_number_count = 0;
    int _previous_display_number = -100;
    int _previous_saved_number = -100;
    const int _same_display_number_threshold = 300;

    float color_bar_brightness = 1.0;
    float numbers_brightness = 1.0;

    void set_pwm_pin(int pin, float val);
    void set_pwm_pin_inverse(int pin, float val);
    bool _is_a_stable_number(int number, bool debounce=true);
    void _set_seven_segment(float digit_1[], float digit_2[]);
    void _update_flash_multiplier();

    unsigned long flash_timestamp = 0;
    unsigned long now;
    float flash_multiplier = 1.0;
    bool is_flashing = false;
    int flash_interval = DEFAULT_FLASH_INTERVAL;
    const float color_bar_min_brightness = 0.1;

    float _color_bar_current[4] = {-1, -1, -1, -1};
    float _color_bar_previous[4] = {-1, -1, -1, -1};
};

#endif
