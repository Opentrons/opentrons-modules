#ifndef Lid_h
#define Lid_h

#include "Arduino.h"
#include <Wire.h>

#if DUMMY_BOARD
  #define PIN_COVER_SWITCH PIN_SPI_MOSI
#else
  #define PIN_COVER_SWITCH      8
#endif
#define PIN_BOTTOM_SWITCH     9

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
#define LID_MOTOR_RANGE_MM  300 // The max distance in mm the motor should move between open to close positions
#define PULSE_HIGH_MICROSECONDS 2

#define TO_INT(an_enum) static_cast<int>(an_enum)

/* The TC has two switches to detect lid positions: one inside the lid (PIN_COVER_SWITCH)
 * that is engaged when the lid fully opens and the other in the main
 * boards assembly (PIN_BOTTOM_SWITCH) which is engaged when the lid is closed
 * and locked. These are N.O. switches and the pins read HIGH when not engaged.
 * When neither of the switch is engaged, the lid is assumed to be 'in_between'
 * 'open' and 'closed' status. When both switches read LOW (which should never happen),
 * the lid is in 'error' state.
 */
#define STATUS_TABLE \
          STATUS(in_between),  \
          STATUS(closed),   \
          STATUS(open),   \
          STATUS(unknown),  \
          STATUS(max)

#define STATUS(_status) _status

enum class Lid_status
{
  STATUS_TABLE
};

#undef STATUS
#define STATUS(_status) #_status

class Lid
{
  public:

    Lid();
    void setup();
    void open_cover();
    void close_cover();
    Lid_status status();
    void solenoid_on();
    void solenoid_off();
    void motor_off();
    void motor_on();
    void set_speed(float mm_per_sec);
    void set_acceleration(float mm_per_sec_per_sec);
    void set_current(float current);
    bool move_millimeters(float mm);
    void check_switches();
    static const char * LID_STATUS_STRINGS[TO_INT(Lid_status::max)+1];

  private:
    bool _is_bottom_switch_pressed;
    bool _is_cover_switch_pressed;
    void _setup_digipot();
    void _save_current();
    void _i2c_write(byte address, byte value);
    void _reset_acceleration();
    void _calculate_step_delay();
    void _update_acceleration();
    void _motor_step(uint8_t dir);
    void _update_status();

    double _step_delay_microseconds = 1000000 / (STEPS_PER_MM * 10);  // default 10mm/sec
    double _mm_per_sec = 20;
    double _acceleration = 300;  // mm/sec/sec
    const double _start_mm_per_sec = 1;
    double _active_mm_per_sec;
    Lid_status _status;
    enum class _Lid_switch {
      cover_switch,
      bottom_switch,
      max
    };
    uint16_t _debounce_state[TO_INT(_Lid_switch::max)] = {0};
};

#endif
