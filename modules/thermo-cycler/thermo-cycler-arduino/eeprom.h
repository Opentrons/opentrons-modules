#ifndef eeprom_h
#define eeprom_h

#include "Arduino.h"
#include <Wire.h>

#define EEPROM_ADDR     0x52  // TODO: make this 0x52 for thermocycler
#define MAX_IN_NUM_LEN  17  // TCV0120190903Annn
#define BUFFER_BYTES    10
#define SERIAL_LOC      10
#define MODEL_LOC       SERIAL_LOC + MAX_IN_NUM_LEN + BUFFER_BYTES
#define VER_NUM_INDEX   3
#define WR_TIME         5   //ms

#define WP_PIN  26

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
