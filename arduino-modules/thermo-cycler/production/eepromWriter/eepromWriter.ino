#include <Wire.h>
#include <ot_shared_data.h>
#include <Arduino.h>

#define WP_PIN                  26
#define PIN_FAN_SINK_CTRL       A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE     2    // Heat sink fan
#define PIN_FRONT_BUTTON_LED    24
#define PIN_PELTIER_ENABLE      7
#define PIN_MOTOR_CURRENT_VREF  A0

String serial_num, model_num;
bool reading_in_serial = false;

bool eeprom_write_char(byte word_address, byte value)
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

char eeprom_read_char(byte word_address)
{
  Wire.beginTransmission(OT_EEPROM_ADDR);
  Wire.write(word_address);
  byte error = Wire.endTransmission();
  if (!error)
  {
    Wire.requestFrom(OT_EEPROM_ADDR, 1); // request 1 byte from eeprom
    if (Wire.available())    // slave may send less than requested
    {
      char c = Wire.read(); // receive a byte as character
      return c;         // print the character
    }
  }
  return '~';
}

bool write_to_eeprom()
{
  /********* Write Serial no. ***********/
  erase_eeprom_section(OT_SERIAL_LOC, OT_MAX_SERIAL_LEN);
  for (uint8_t i = 0; i < OT_MAX_SERIAL_LEN; i++)
  {
    if(!eeprom_write_char(byte(OT_SERIAL_LOC+i), byte(serial_num.charAt(i))))
    {
      Serial.println("\nfailed");
      return false;
    }
  }
  /********** Write Model no. **********/
  erase_eeprom_section(OT_MODEL_LOC, OT_MAX_MODEL_LEN);
  for (uint8_t i = 0; i < OT_MAX_MODEL_LEN; i++)
  {
    if(!eeprom_write_char(byte(OT_MODEL_LOC+i), byte(model_num.charAt(i))))
    {
      Serial.println("failed");
      return false;
    }
  }
  return true;
}

bool read_from_eeprom()
{
  uint8_t addr;
  String ser_read;
  String model_read;

  /********* Serial number **********/
  addr = OT_SERIAL_LOC;
  ser_read.reserve(OT_MAX_SERIAL_LEN);
  while (ser_read.length() < OT_MAX_SERIAL_LEN)
  {
    char c = eeprom_read_char(addr++);
    if (c == '0xff')
    {
      break;
    }
    ser_read += c;
  }
  /********* Model number ***********/
  addr = OT_MODEL_LOC;
  model_read.reserve(OT_MAX_MODEL_LEN);
  while (model_read.length() < OT_MAX_MODEL_LEN)
  {
    char c = eeprom_read_char(addr++);
    if (c == '0xff')
    {
      break;
    }
    model_read += c;
  }

  Serial.print("Serial:"); Serial.println(ser_read);
  Serial.print("Model:"); Serial.println(model_read);
  return true;
}

bool erase_eeprom_section(uint8_t addr, uint8_t sec_len)
{
  for (uint8_t i = 0; i < sec_len; i++)
  {
    if(!eeprom_write_char(byte(addr+i), byte(0xff)))  // erasing = writing 0xffh
    {
      Serial.println("\nfailed to erase section");
      return false;
    }
  }
  return true;
}

void setup_tc_peripherals()
{
  pinMode(PIN_FAN_SINK_ENABLE, OUTPUT);
  digitalWrite(PIN_FAN_SINK_ENABLE, LOW);
  pinMode(PIN_PELTIER_ENABLE, OUTPUT);
  digitalWrite(PIN_PELTIER_ENABLE, LOW);
  analogWrite(PIN_MOTOR_CURRENT_VREF, 0);
}

void setup()
{
  // pin initializations for thermocycler peripherals
  setup_tc_peripherals();
  pinMode(WP_PIN, OUTPUT);
  digitalWrite(WP_PIN, LOW);  // Disables write protect
  Serial.begin(115200);
  delay(10);
  Wire.begin();
  serial_num.reserve(OT_MAX_SERIAL_LEN);
  model_num.reserve(OT_MAX_MODEL_LEN);
}

void loop()
{
  if (Serial.available() > 0) {
    serial_num = "";
    model_num = "";
		reading_in_serial = true;
		char in_char;
		while (Serial.available() > 0) {
      delay(2);
			in_char = Serial.read();
      switch(in_char)
      {
        case '&':
          read_from_eeprom();
          break;
        case ':':
          reading_in_serial = false;  // Done reading serial. Stored in serial_num
          break;
        case '\r':
        case '\n':
          break;    // ignore newline and carriage return
        default:
          // Not a special character. Should be either serial or model num character
          if (reading_in_serial)
          {
            serial_num += in_char;
          }
          else
          {
            model_num += in_char;
          }
          break;
      }
		}
    if (serial_num.length() && model_num.length()) {
  		write_to_eeprom();
    }
    Serial.println();  // acknowledgement
	}
}
