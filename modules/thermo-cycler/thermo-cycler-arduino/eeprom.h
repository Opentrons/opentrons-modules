#ifndef eeprom_h
#define eeprom_h

#include "Arduino.h"
#include <Wire.h>

#define WP_PIN  26
#define A_LOC   50
#define B_LOC   60
#define C_LOC   70

enum class MemOption
{
  serial,
  model
};

enum class OffsetConst
{
  A,
  B,
  C
};

/* EEPROM class can do the following:
 * 1. Read the stored Serial and Model numbers <`read(option)`>
 * 2. Read and write values of temp offset constants: a, b, c
 *    <`set_offset(constant, value)`> and <`get_offset(constant)`>
 * Reading and writing of all the above parameters is initiated using gcodes.
 */
class Eeprom
{
  public:
    Eeprom();
    String read(MemOption option);
    void setup();
    bool set_offset(OffsetConst c, float val);
    float get_offset(OffsetConst c);
  private:
    char _read_char(uint8_t word_address);
    bool _write_char(uint8_t word_address, uint8_t value);
    bool _erase_eeprom_section(uint8_t word_address, uint8_t sec_len);
};
#endif
