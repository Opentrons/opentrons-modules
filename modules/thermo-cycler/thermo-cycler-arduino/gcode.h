#ifndef GCODE_H
#define GCODE_H

#include "Arduino.h"
#include <cerrno>
#include <cstdlib>

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
  GCODE_DEF(dfu, dfu),              \
  GCODE_DEF(max, -)

#define GCODE_DEF(name, _) name

enum class Gcode
{
  GCODES_TABLE
};

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
