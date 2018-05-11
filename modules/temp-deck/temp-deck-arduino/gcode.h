#ifndef Gcode_h
#define Gcode_h

#include "Arduino.h"

#define NO_TARGET_SET 200
#define ROOM_TEMPERATURE 25

#define TOTAL_GCODE_COMMAND_CODES 5


class Gcode{
    public:

        Gcode();
        void update();

    private:

        String COMMAND_CODES[TOTAL_GCODE_COMMAND_CODES] = {
          "M105", "M104", "M18", "M115", "dfu"
        };

        String SERIAL_BUFFER_STRING;
        char CHARACTERS_TO_STRIP[3] = {' ', '\r', '\n'};

        int target_temp = NO_TARGET_SET;
        int current_temp = ROOM_TEMPERATURE;

        const int MILLIS_PER_DEGREE = 100;
        unsigned long degree_change_timestamp = 0;
        unsigned long now = 0;

        void _send_ack();
        void _strip_serial_buffer();
        void _parse_temp_from_buffer();
        void _print_current_temperature();
        void _print_model_and_version();
        void _parse_gcode_commands();
};

#endif
