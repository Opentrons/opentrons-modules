#ifndef Memory_h
#define Memory_h

#include "Arduino.h"

#include <EEPROM.h>

// Sample EEPROM representation after being written to:
// 0x00     0 1 2 3 4 5 6 7 8 9 A B C \0 \0 \0...       Serial Number
// 0x20     t e m p - v 1 . 3 . 4 \0 \0 \0 \0 \0...     Model number
// 0x40     1 a 2 b 3 c 4 d 5 e 6 f 7 8 9 0...          32-bit crc (serial)
// 0x60     a b c d e f 1 2 3 \0 \0 \0 \0 \0 \0...      32-bit crc (model)

// Assuming both ID & Model no. are not more than 16 bytes long
#define DEVICE_SERIAL_ADDR      0x00
#define DEVICE_MODEL_ADDR       0x20
#define SERIAL_CRC_ADDR         0x40
#define MODEL_CRC_ADDR          0x60
#define DATA_MAX_LENGTH         32

#define ERR_FLAG_DATA_TO_LONG   1
#define ERR_FLAG_EEPROM_INVALID 2

class Memory{

    public:

        Memory();
        uint8_t setup();
        uint8_t error();
        void write_serial();
        void write_model();
        void read_serial();
        void read_model();
        void erase_serial_data();
        void erase_model_data();

        String serial;
        String model;
        String writeData;

    private:

        const unsigned long crc_table[16] = {
            0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
            0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
            0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
            0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
        };

        enum IDs{
            SERIAL_NUM,
            MODEL_NUM
        };

        uint8_t error_flag;

        unsigned long calculate_crc(uint32_t address);
        bool check_eeprom_validity(int ID_type);
        void set_error_flag(int flag);
        void read_from_eeprom(int ID_type);
        void write_to_eeprom(uint32_t address);
        void update_serial_crc();
        void update_model_crc();
        void read_gcode();
        uint32_t id_type_to_address(int ID_type);
};

#endif
