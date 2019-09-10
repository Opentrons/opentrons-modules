#ifndef eeprom_h
#define eeprom_h

#include "Arduino.h"
#include <Wire.h>

#define WP_PIN   26

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
