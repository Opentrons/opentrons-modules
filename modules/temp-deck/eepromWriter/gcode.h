/*


Gcode gcode = Gcode(115200);

if (gcode.received_newline()) {
    while (gcode.pop_command()) {
        switch(gcode.code) {
            case GCODE_GET_TEMP:
                gcode.print_targetting_temperature(target, current);
                break;
            case GCODE_SET_TEMP:
                if (gcode.parse_int('S')) {
                    target = gcode.parsed_int;
                }
                break;
            case GCODE_DISENGAGE:
                off()
                break;
            case GCODE_DEVICE_INFO:
                gcode.print_device_info(serial, model, version);
                break;
            case GCODE_DFU:
                start_dfu_timeout();
                break;
            default:
                break;
        }
    }
    gcode.send_ack();
}

*/

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

/*
READ_DEVICE_ID: M369
WRITE_DEVICE_ID: M370
READ_DEVICE_MODEL: M371
WRITE_DEVICE_MODEL: M372
*/

// SPP
#define GCODE_READ_DEVICE_SERIAL    5
#define GCODE_WRITE_DEVICE_SERIAL   6
#define GCODE_READ_DEVICE_MODEL     7
#define GCODE_WRITE_DEVICE_MODEL    8

#define TOTAL_GCODE_COMMAND_CODES   9

class Gcode{

    public:

        Gcode();
        void setup(int baudrate);
        bool received_newline();
        bool pop_command();
        int code;
        void send_ack();
        int parsed_int;
        void print_device_info(String serial, String model, String version);
        void print_targetting_temperature(int target_temp, int current_temp);
        void print_stablizing_temperature(int current_temp);
        bool read_int(char key);
        void print_warning(String msg);
        String* read_parameter();

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
