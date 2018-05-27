#ifndef Gcode_h
#define Gcode_h

#include "Arduino.h"

#define NO_TARGET_TEMP_SET 32766

#define MAX_SERIAL_BUFFER_LENGTH 500

#define GCODE_NO_CODE               -1
#define GCODE_GET_TEMP              0
#define GCODE_SET_TEMP              1
#define GCODE_DISENGAGE             2
#define GCODE_DEVICE_INFO           3
#define GCODE_DFU                   4
#define TOTAL_GCODE_COMMAND_CODES   5

class Gcode{

    public:

        Gcode();
        void setup(int baudrate);
        bool received_newline();
        bool pop_command();
        void send_ack();
        int code;
        float parsed_number = 0;
        void print_device_info(String serial, String model, String version);
        void print_targetting_temperature(int target_temp, int current_temp);
        void print_stablizing_temperature(int current_temp);
        bool read_number(char key);
        void print_warning(String msg);

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
