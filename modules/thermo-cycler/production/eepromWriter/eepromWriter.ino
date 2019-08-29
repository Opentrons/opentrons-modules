#include <Wire.h>

#define EEPROM_ADDR 0x52
#define SERIAL_LOC  0x100
#define CONFIG_LOC  0x200
#define WR_TIME     5 //ms

#define WP_PIN  26  // TODO: check this pin setup in bootloader

bool eeprom_write(byte word_address, byte value)
{
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(word_address);
  Wire.write(value);
  delay(T_WR);
  byte error = Wire.endTransmission();
  return (error ? false : true);
}

byte eeprom_read()
{
  Wire.requestFrom(EEPROM_ADDR, 1); // request 1 byte from eeprom
  delay(10);
  if (Wire.available())
  {
    byte r = Wire.read();
    return r;
  }
  return -1;
}

void setup()
{
  pinMode(WP_PIN, OUTPUT);
  digitalWrite(WP_PIN, LOW);  // Disables write protect
  Serial.begin(115200);
  while (Serial.available() > 0)
  {
    Serial.read();
    delay(2);
  }
}

void loop()
{

}
