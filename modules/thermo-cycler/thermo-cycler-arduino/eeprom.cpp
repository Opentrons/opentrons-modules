#include "eeprom.h"

Eeprom::Eeprom()
{}

String Eeprom::read(MemOption option)
{
#if HW_VERSION >= 4
  uint8_t addr;
  if (option == MemOption::serial)
  {
    addr = SERIAL_LOC;
  }
  else if (option == MemOption::model)
  {
    addr = MODEL_LOC;
  }
  else
  {
    return "\0";
  }
  String the_number;
  while (_read_char(addr) != '\0')
  {
    the_number += _read_char(addr++);
  }
  Serial.println(the_number);
  return the_number;
#endif
}

char Eeprom::_read_char(uint8_t word_address)
{
#if HW_VERSION >= 4
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(word_address);
  uint8_t error = Wire.endTransmission();
  if (!error)
  {
    Wire.requestFrom(EEPROM_ADDR, 1); // request 1 byte from eeprom
    if (Wire.available())    // slave may send less than requested
    {
      char c = Wire.read(); // receive a byte as character
      return c;         // print the character
    }
  }
  return '~';
#endif
}

void Eeprom::setup()
{
  Wire.begin();
}
