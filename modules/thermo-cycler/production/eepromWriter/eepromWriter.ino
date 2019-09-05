#include <Wire.h>

#define EEPROM_ADDR 0x50 // TODO: make this 0x52 for thermocycler
#define SERIAL_LOC  10
#define MAX_SER_NUM_LEN 20 //TCV0120190903Annnn
#define MODEL_LOC   (SERIAL_LOC+MAX_SER_NUM_LEN+10)
#define CONFIG_LOC  MODEL_LOC + 50
#define WR_TIME     5 //ms

#define WP_PIN  5  // TODO: check this pin setup in bootloader

#define MAX_MODEL_NUM_LEN 8 //vxx.yy.zz // Includes '.'

enum class MemOption
{
  none,
  read_serial,
  write_serial,
  read_model,
  write_model,
};


MemOption input_option = MemOption::none;
String serial_num;
String model_num;

bool eeprom_write_char(byte word_address, byte value)
{
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(word_address);
  Wire.write(value);
  byte error = Wire.endTransmission();
  delay(WR_TIME);
  if (error)
  {
    return false;
  }
  return true;
}

char eeprom_read_char(byte word_address)
{
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(word_address);
  byte error = Wire.endTransmission();
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
}

void setup()
{
  pinMode(WP_PIN, OUTPUT);
  digitalWrite(WP_PIN, LOW);  // Disables write protect
  Serial.begin(115200);
  delay(10);
  Wire.begin();
}

bool write_to_eeprom(MemOption option)
{
  char the_number[MAX_SER_NUM_LEN];
  uint8_t number_len;
  uint8_t addr;
  if (option == MemOption::write_serial)
  {
    addr = SERIAL_LOC;
    serial_num.toCharArray(the_number, serial_num.length()+1);
    number_len = serial_num.length()+1;
  }
  else if (option == MemOption::write_model)
  {
    addr = MODEL_LOC;
    model_num.toCharArray(the_number, model_num.length()+1);
    number_len = model_num.length()+1;
  }
  else
  {
    return 0;
  }
  Serial.println("writing to eeprom..");
  for (uint8_t i = 0; i < number_len; i++)
  {
    Serial.print(the_number[i]);
    if(!eeprom_write_char(byte(addr+i), byte(the_number[i])))
    {
      Serial.println("\nfailed");
      return 0;
    }
  }
  Serial.println("\ndone!");
  return true;
}

bool read_from_eeprom(MemOption option)
{
  uint8_t addr;
  if (option == MemOption::read_serial)
  {
    addr = SERIAL_LOC;
  }
  else if (option == MemOption::read_model)
  {
    addr = MODEL_LOC;
  }
  else
  {
    return 0;
  }
  String the_number;
  while (eeprom_read_char(addr) != '\0')
  {
    the_number += eeprom_read_char(addr++);
  }
  // char c;
  // do
  // {
  //   c = eeprom_read_char(addr);
  //   the_number += c;
  //   addr++;
  // }
  // while(c != '\0');
  Serial.println(the_number);
}

////////////////////////////////////////////

void read_input()
{
  if (Serial.available())
  {
    String in_option;
    in_option = Serial.readStringUntil('\n');
    in_option.trim();
    in_option.toLowerCase();
    Serial.println(in_option);

    if (in_option == "serial" || in_option == "model")
    {
      String option = in_option;
      Serial.println("W(write) or R(read)?");
      while(!Serial.available());
      char mode = Serial.read();

      if (mode == 'W' or mode == 'w')
      {
        while (Serial.available())
        {
          Serial.read();  // empty out the buffer for serial number
        }

        Serial.println("Enter the number");
        while (!Serial.available());
        String in_string = Serial.readStringUntil('\n');
        // Scanned/Input number is going to end with '\r\n'
        in_string.trim();
        Serial.print("Received:"); Serial.println(in_string);
        if (option == "serial")
        {
          serial_num = in_string;
          input_option = MemOption::write_serial;
        }
        else
        {
          model_num = in_string;
          input_option = MemOption::write_model;
        }
      }
      else if (mode == 'R' or mode == 'r')
      {
        while (Serial.available())
        {
          Serial.read();  // empty out the buffer for serial number
        }
        if (option == "serial")
        {
          input_option = MemOption::read_serial;
        }
        else
        {
          input_option = MemOption::read_model;
        }
      }
      else
      {
        Serial.println("Invalid input");
        input_option = MemOption::none;
      }
    }
    else
    {
      send_ack();
    }
  }
}

void send_ack()
{
  Serial.println("ok");
  Serial.println("ok");
}

void loop()
{
  // Serial.println(".");
  delay(500);
  // report_input();
  read_input();
  switch (input_option)
  {
    case MemOption::write_serial:
      // TODO: erase section before writing
      write_to_eeprom(MemOption::write_serial);
      input_option = MemOption::none;
      break;
    case MemOption::write_model:
      // TODO: erase section before writing
      write_to_eeprom(MemOption::write_model);
      input_option = MemOption::none;
      break;
    case MemOption::read_serial:
      read_from_eeprom(MemOption::read_serial);
      input_option = MemOption::none;
      break;
    case MemOption::read_model:
      read_from_eeprom(MemOption::read_model);
      input_option = MemOption::none;
      break;
    case MemOption::none:
    default:
      break;
  }
}
