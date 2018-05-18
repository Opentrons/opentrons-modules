#ifndef Gcode_h
#define Gcode_h

#include "Arduino.h"

#define NO_TARGET_TEMP_SET 32766

#define MAX_SERIAL_BUFFER_LENGTH 500

enum CODES{
    GCODE_NO_CODE = -1,
    GCODE_GET_TEMP = 0,
    GCODE_SET_TEMP,
    GCODE_DISENGAGE,
    GCODE_DEVICE_INFO,
    GCODE_DFU,
    GCODE_READ_DEVICE_SERIAL,
    GCODE_WRITE_DEVICE_SERIAL,
    GCODE_READ_DEVICE_MODEL,
    GCODE_WRITE_DEVICE_MODEL
};

#define TOTAL_GCODE_COMMAND_CODES   9

class Gcode{

    public:

        Gcode();
        void setup(int baudrate);
        bool received_newline();
        bool pop_command();
        void send_ack();
        int code;
        int parsed_int;
        void print_device_info(String serial, String model, String version);
        void print_targetting_temperature(int target_temp, int current_temp);
        void print_stablizing_temperature(int current_temp);
        bool read_int(char key);
        String* read_parameter();
        void print_warning(String msg);

    private:

        String COMMAND_CODES[TOTAL_GCODE_COMMAND_CODES];
        String GCODE_BUFFER_STRING;
        String SERIAL_BUFFER_STRING;
        String parameter_string;
        char CHARACTERS_TO_STRIP[3] = {' ', '\r', '\n'};
        bool SEND_ACK_NEXT_UPDATE = false;

        void _strip_serial_buffer();
        void _parse_gcode_commands();
};

#endif
