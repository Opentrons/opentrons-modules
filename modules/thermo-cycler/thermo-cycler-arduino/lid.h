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

#if HW_VERSION >= 3
  #define PIN_MOTOR_CURRENT_VREF        A0
  #define PIN_MOTOR_FAULT               22
  #define PIN_MOTOR_RST                 38
#endif

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

// Current setting values calculated from datasheet
#define CURRENT_SETTING_1 12  // Vout = 0.08V
#define CURRENT_SETTING_2 85  // Vout = 0.55V
#define CURRENT_SETTING_3 64  // Vout = 0.3945 (approx)
#define CURRENT_SETTING_4 77  // Vout = 0.48  (Imax=1.2)
#define CURRENT_SETTING_5 99  // Vout = 0.7V (calculated. Not observed)

#define CURRENT_SETTING CURRENT_SETTING_4
#define AD5110_SET_VALUE_CMD 0x02
#define AD5110_SAVE_VALUE_CMD 0x01
#define AD5110_READ_TOLERANCE_CMD 0x06

#define MOTOR_CURRENT_VREF 0.48

#define CURRENT_HIGH 0.5
#define CURRENT_LOW 0.01
#define MOTOR_ENABLE_DELAY_MS 5

#if HW_VERSION >=3
#define DIRECTION_DOWN LOW
#define DIRECTION_UP HIGH
#else
#define DIRECTION_DOWN HIGH
#define DIRECTION_UP LOW
#endif

#define STEP_ANGLE  1.8
#define MICRO_STEP  32
#define MOTOR_REDUCTION_RATIO  99.5
#define STEPS_PER_ANGLE  uint16_t((MICRO_STEP * MOTOR_REDUCTION_RATIO) / STEP_ANGLE)
#define STEPS_PER_MM 480  // full-stepping
#define LID_MOTOR_RANGE_MM  390 // The max distance in mm the motor should move between open to close positions
#define LID_MOTOR_RANGE_DEG 100 // Max angle the lid motor can move
#define PULSE_HIGH_MICROSECONDS 2
#define MOTOR_STEP_DELAY 60   // microseconds
#define LID_CLOSE_BACKTRACK_ANGLE 3.0
#define LID_CLOSE_LAST_STEP_ANGLE 1.5
#define LID_OPEN_SWITCH_PROBE_ANGLE -30
#define LID_OPEN_DOWN_MOTION_ANGLE 4.0

#define TO_INT(an_enum) static_cast<int>(an_enum)

/* The TC has two switches to detect lid positions: one inside the lid (PIN_COVER_SWITCH)
 * that is engaged when the lid fully opens and the other in the main
 * boards assembly (PIN_BOTTOM_SWITCH) which is engaged when the lid is closed
 * and locked. These are N.C. switches and the pins read LOW when not engaged.
 * When neither of the switch is engaged, the lid is assumed to be 'in_between'
 * 'open' and 'closed' status. When both switches read HIGH (which should never happen),
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
    bool setup();
    bool open_cover();
    bool close_cover();
    Lid_status status();
    void solenoid_on();
    void solenoid_off();
    void motor_off();
    void motor_on();
    bool move_angle(float deg);
    void check_switches();
    void reset_motor_driver();
    bool is_driver_faulted();
    static const char * LID_STATUS_STRINGS[TO_INT(Lid_status::max)+1];
    float _read_tolerance();

  private:
    bool _is_bottom_switch_pressed;
    bool _is_cover_switch_pressed;
    bool _setup_digipot();
    bool _save_current();
    bool _set_current(uint8_t current_data);
    bool _i2c_write(byte address, byte value);
    void _motor_step(uint8_t dir);
    void _update_status();
    byte _i2c_read();
    uint16_t _to_dac_out(float driver_vref);

    Lid_status _status;
    enum class _Lid_switch {
      cover_switch,
      bottom_switch,
      max
    };
    uint16_t _debounce_state[TO_INT(_Lid_switch::max)] = {0};
    float digipot_tolerance;
};

#endif
