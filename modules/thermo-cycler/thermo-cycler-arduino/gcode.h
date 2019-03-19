#ifndef GCODE_H
#define GCODE_H

#include "Arduino.h"

#define CODE_INT(gcode) static_cast<int>(gcode)

#define MAX_SERIAL_BUFFER_LENGTH    100
#define MAX_SERIAL_DIGITS_IN_NUMBER 7
#define SERIAL_DIGITS_IN_RESPONSE   3

#define GCODES_TABLE  \
  GCODE_DEF(no_code, -),            \
  GCODE_DEF(get_lid_status, M119),  \
  GCODE_DEF(open_lid, M126),        \
  GCODE_DEF(close_lid, M127),       \
  GCODE_DEF(set_lid_temp, M140),    \
  GCODE_DEF(deactivate_lid_heating, M108),  \
  GCODE_DEF(set_plate_temp, M104),  \
  GCODE_DEF(get_plate_temp, M105),  \
  GCODE_DEF(set_ramp_rate, M566),   \
  GCODE_DEF(edit_pid_params, M301), \
  GCODE_DEF(pause, M76),            \
  GCODE_DEF(deactivate_all, M18),   \
  GCODE_DEF(get_device_info, M115), \
  GCODE_DEF(dfu, dfu),              \
  GCODE_DEF(max, -)

#define GCODE_DEF(name, _) name

enum class Gcode
{
  GCODES_TABLE
};

/* This class sets up the serial port to receive gcodes and send responses.
 * A valid gcode would be in the format:
 *         <gcode1> <gcode1_arg1> <gcode1_arg2> <gcode2> <gcode2_arg1> \r\n
 * Note that each serial input line can contain as many number of gcodes and
 * their arguments as the Serial Buffer Size allows, however, the line should end
 * with a `\r\n`. Spaces can be skipped since they are ignored.
 *
 * To use a GCodeHandler object, use `setup(baudrate)` to start serial communication,
 * then in a loop, check for `received_newline()` and use `get_command()`
 * until `buffer_empty()` to cache the received gcode (fifo order) and its arguments.
 * Use `pop_arg(key)` to cache the desired keyed argument and read the argument
 * using `popped_arg()`.
 * Responses can be sent using the appropriate 'response' methods below.
 */
class GcodeHandler
{
    public:
      GcodeHandler();
      void setup(int baudrate);
      bool received_newline();
      Gcode get_command();
      void send_ack();
      bool buffer_empty();
      void device_info_response(String serial, String model, String version);
      void targetting_temperature_response(float target_temp,
                                           float current_temp, float time_left);
      void idle_temperature_response(float current_temp);
      float popped_arg();
      bool pop_arg(char key);
      void response(String msg);
      void response(String param, String msg);

    private:
      struct
      {
        Gcode code;
        String args_string;
      }_command;
      String _gcode_buffer_string;
      String _serial_buffer_string;
      void _strip_serial_buffer();
      float _parsed_arg;
      bool _find_command(String, uint8_t *, uint8_t *);
};

#endif
