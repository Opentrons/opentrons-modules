#include "eeprom.h"
#include <ot_shared_data.h>

Eeprom::Eeprom()
{}

String Eeprom::read(MemOption option)
{
#if HW_VERSION >= 4
  uint8_t addr;
  String the_number;
  if (option == MemOption::serial)
  {
    addr = OT_SERIAL_LOC;
    the_number.reserve(OT_MAX_SERIAL_LEN);
    while (the_number.length() < OT_MAX_SERIAL_LEN)
    {
      the_number += _read_char(addr++);
    }
  }
  else if (option == MemOption::model)
  {
    addr = OT_MODEL_LOC;
    the_number.reserve(OT_MAX_MODEL_LEN);
    while (the_number.length() < OT_MAX_MODEL_LEN)
    {
      the_number += _read_char(addr++);
    }
  }
  else
  {
    return "\0";
  }

  return the_number;
#else
  if (option == MemOption::serial)
  {
    return "dummySerial";
  }
  else if (option == MemOption::model)
  {
    return "dummyModel";
  }
  else
  {
    return "\0";
  }
#endif
}

char Eeprom::_read_char(uint8_t word_address)
{
#if HW_VERSION >= 4
  Wire.beginTransmission(OT_EEPROM_ADDR);
  Wire.write(word_address);
  uint8_t error = Wire.endTransmission();
  if (!error)
  {
    Wire.requestFrom(OT_EEPROM_ADDR, 1); // request 1 byte from eeprom
    if (Wire.available())    // slave may send less than requested
    {
      return Wire.read(); // receive a byte as character
    }
  }
  return '~';
#endif
}

void Eeprom::setup()
{
  pinMode(WP_PIN, INPUT);   // Tristate so WP = Vcc (WP_PIN pulled up in HW)
  Wire.begin();
}
