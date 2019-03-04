#ifndef GCODE_H
#define GCODE_H

#include "Arduino.h"

#define TOTAL_GCODE_COMMAND_CODES   11
#define MAX_SERIAL_BUFFER_LENGTH    100
#define MAX_SERIAL_DIGITS_IN_NUMBER 7
#define SERIAL_DIGITS_IN_RESPONSE   3

typedef enum{
  GCODE_NO_CODE = -1,
  GCODE_GET_LID_STATUS = 0,
  GCODE_OPEN_LID,
  GCODE_CLOSE_LID,
  GCODE_SET_LID_TEMP,
  GCODE_DEACTIVATE_LID_HEATING,
  GCODE_SET_PLATE_TEMP,
  GCODE_GET_PLATE_TEMP,
  GCODE_SET_RAMP_RATE,
  GCODE_EDIT_PID_PARAMS,
  GCODE_PAUSE,
  GCODE_DEACTIVATE_ALL,
  GCODE_DFU
}Gcode_enum;

class Gcode{
    public:
        Gcode();
        void setup(int baudrate);
        bool received_newline();
        bool pop_command();
        void send_ack();
        int code;
        float parsed_number = 0;
        void device_info_response(String serial, String model, String version);
        void targetting_temperature_response(float target_temp,
                                             float current_temp, float time_left);
        void idle_temperature_response(float current_temp);
        bool read_number(char key);
        void response(String msg);
        void response(String param, String msg);

    private:

        String COMMAND_CODES[TOTAL_GCODE_COMMAND_CODES];
        String GCODE_BUFFER_STRING;
        String SERIAL_BUFFER_STRING;
        char CHARACTERS_TO_STRIP[3] = {' ', '\r', '\n'};
        bool SEND_ACK_NEXT_UPDATE = false;

        void _strip_serial_buffer();
        void _parse_gcode_commands();
};

#endif
