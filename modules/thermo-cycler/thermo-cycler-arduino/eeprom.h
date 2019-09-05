#ifndef eeprom_h
#define eeprom_h

#include "Arduino.h"
#include <Wire.h>

#define EEPROM_ADDR     0x52  // TODO: make this 0x52 for thermocycler
#define SERIAL_LOC      10
#define MAX_SER_NUM_LEN 20  // TCV0120190903Annnn
#define MODEL_LOC       SERIAL_LOC+MAX_SER_NUM_LEN+10
#define CONFIG_LOC      MODEL_LOC + 50
#define WR_TIME         5   //ms

#define WP_PIN  26  // TODO: check this pin setup in bootloader

enum class MemOption
{
  serial,
  model
};

class Eeprom
{
  public:
    Eeprom();
    String read(MemOption option);
    void setup();
  private:
    char _read_char(uint8_t word_address);
};
#endif
