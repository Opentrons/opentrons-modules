#ifndef eeprom_h
#define eeprom_h
#if HW_VERSION >= 4

// #include "Arduino.h"
#include <Wire.h>

#define EEPROM_ADDR 0x52
#define SERIAL_LOC  0x100
#define CONFIG_LOC  0x200

#define WP_PIN  26  // TODO: check this pin setup in bootloader

class Eeprom
{
  public:
    String read_serial();
    String get_tc_model();
    bool write_config();
    bool read_config();
    void setup();
};
#endif
#endif
