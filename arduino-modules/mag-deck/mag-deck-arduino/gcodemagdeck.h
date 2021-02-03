#ifndef GcodeMagDeck_h
#define GcodeMagDeck_h

#include "Arduino.h"

#define NO_TARGET_TEMP_SET 32766

#define MAX_SERIAL_BUFFER_LENGTH 100
#define MAX_SERIAL_DIGITS_IN_NUMBER 7
#define SERIAL_DIGITS_IN_RESPONSE 3

#define GCODE_NO_CODE               -1
#define GCODE_HOME                  0
#define GCODE_MOVE                  1
#define GCODE_PROBE                 2
#define GCODE_GET_PROBED_DISTANCE   3
#define GCODE_GET_POSITION          4
#define GCODE_DEVICE_INFO           5
#define GCODE_DFU                   6
#define TOTAL_GCODE_COMMAND_CODES   7

class GcodeMagDeck{

    public:

        GcodeMagDeck();
        void setup(int baudrate);
        bool received_newline();
        bool pop_command();
        void send_ack();
        int code;
        float parsed_number = 0;
        void print_device_info(String serial, String model, String version);
        void print_current_position(float mm);
        void print_probed_distance(float mm);
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
