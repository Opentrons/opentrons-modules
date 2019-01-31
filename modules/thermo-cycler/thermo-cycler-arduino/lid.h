#ifndef Lid_h
#define Lid_h

#include "Arduino.h"
#include <Wire.h>

#define PIN_COVER_OPEN_SWITCH       8
#define PIN_COVER_CLOSED_SWITCH     9

#define PIN_SOLENOID                A1

#define PIN_STEPPER_STEP            1
#define PIN_STEPPER_DIR             3
#define PIN_STEPPER_ENABLE          0

#define STEPPER_OFF_STATE HIGH
#define STEPPER_ON_STATE LOW

#define SWITCH_PRESSED_STATE HIGH

#define SOLENOID_STATE_ON HIGH
#define SOLENOID_STATE_OFF LOW

#define SOLENOID_TIME_TO_OPEN_MILLISECONDS 20
#define SOLENOID_ENGAGED_MILLISECONDS 100

#define ADDRESS_DIGIPOT 0x2C  // 7-bit address
#define SET_CURRENT_DELAY_MS 20
#define AD5110_CURRENT_SCALER 1.0
#define AD5110_SET_VALUE_CMD 0x02
#define AD5110_SAVE_VALUE_CMD 0x01

#define CURRENT_HIGH 0.5
#define CURRENT_LOW 0.01
#define MOTOR_ENABLE_DELAY_MS 5

#define DIRECTION_DOWN LOW
#define DIRECTION_UP HIGH

#define STEPS_PER_MM 15  // full-stepping
#define PULSE_HIGH_MICROSECONDS 2


class Lid{
  public:

    Lid();
    void setup();
    void open_cover();
    void close_cover();
    bool is_lid_open();
    bool is_lid_closed();
    void solenoid_on();
    void solenoid_off();
    void motor_off();
    void motor_on();
    void set_speed(float mm_per_sec);
    void set_acceleration(float mm_per_sec_per_sec);
    void set_current(float current);
    void move_millimeters(float mm, bool top_switch, bool bottom_switch);

  private:

    void wait_to_hit_bottom();
    void wait_to_hit_top();
    void setup_digipot();
    void save_current();
    void i2c_write(byte address, byte value);
    void reset_acceleration();
    void calculate_step_delay();
    void update_acceleration();
    void motor_step(uint8_t dir);

    double STEP_DELAY_MICROSECONDS = 1000000 / (STEPS_PER_MM * 10);  // default 10mm/sec
    double MM_PER_SEC = 20;
    double ACCELERATION = 300;  // mm/sec/sec
    const double _start_mm_per_sec = 1;
    double _active_mm_per_sec;
};

#endif
