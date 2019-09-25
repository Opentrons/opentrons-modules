#include "eeprom.h"
#include <ot_shared_data.h>

union ConstValue
{
  float f_val;
  byte b_val[sizeof(f_val)];
};

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

bool Eeprom::_write_char(uint8_t word_address, uint8_t value)
{
  Wire.beginTransmission(OT_EEPROM_ADDR);
  Wire.write(word_address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  delay(OT_WR_TIME);
  if (error)
  {
    return false;
  }
  return true;
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
#endif
  return '~';
}

bool Eeprom::set_offset(OffsetConst constant, float val)
{
  pinMode(WP_PIN, OUTPUT);
  digitalWrite(WP_PIN, LOW); // Disables write protect
  ConstValue this_const;
  this_const.f_val = val;
  uint8_t addr;
  switch(constant)
  {
    case OffsetConst::A:
      addr = A_LOC;
      break;
    case OffsetConst::B:
      addr = B_LOC;
      break;
    case OffsetConst::C:
      addr = C_LOC;
      break;
  }
  _erase_eeprom_section(addr, uint8_t(sizeof(this_const.f_val)));
  for (uint8_t i = 0; i < sizeof(this_const.f_val); i++)
  {
    if(!_write_char(byte(addr+i), byte(this_const.b_val[i])))
    {
      return false; // failed
    }
  }
  pinMode(WP_PIN, INPUT);
}

bool Eeprom::_erase_eeprom_section(uint8_t addr, uint8_t sec_len)
{
  for (uint8_t i = 0; i < sec_len; i++)
  {
    if(!_write_char(byte(addr+i), byte(0xff)))  // erasing = writing 0xffh
    {
      Serial.println("\nfailed to erase section");
      return false;
    }
  }
  return true;
}

float Eeprom::get_offset(OffsetConst constant)
{
  ConstValue this_const;
  uint8_t addr;
  switch(constant)
  {
    case OffsetConst::A:
      addr = A_LOC;
      break;
    case OffsetConst::B:
      addr = B_LOC;
      break;
    case OffsetConst::C:
      addr = C_LOC;
      break;
  }
  for (uint8_t i = 0; i < sizeof(this_const.f_val); i++)
  {
    this_const.b_val[i] = _read_char(addr+i);
  }
  return this_const.f_val;
}

void Eeprom::setup()
{
  pinMode(WP_PIN, INPUT);   // Tristate so WP = Vcc (WP_PIN pulled up in HW)
  Wire.begin();
}
