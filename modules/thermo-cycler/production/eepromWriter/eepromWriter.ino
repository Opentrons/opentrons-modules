#include <Wire.h>

#define EEPROM_ADDR       0x52

#define MAX_IN_NUM_LEN    17  // max serial = 17 (TCV0120190903Annn) | max model = 3 //vnn
#define SERIAL_LOC        10
#define BUFFER_BYTES      10
#define MODEL_LOC         SERIAL_LOC + MAX_IN_NUM_LEN + BUFFER_BYTES
#define VER_NUM_INDEX     3
#define VER_NUM_LEN       2
#define WR_TIME           5 //ms

#define WP_PIN                  26
#define PIN_FAN_SINK_CTRL       A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE     2    // Heat sink fan
#define PIN_FRONT_BUTTON_LED    24
#define PIN_PELTIER_ENABLE      7
#define PIN_MOTOR_CURRENT_VREF  A0

bool options_onscreen;

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

bool write_to_eeprom(String serial_num)
{
  char the_number[MAX_IN_NUM_LEN];
  uint8_t number_len;
  serial_num.toCharArray(the_number, serial_num.length()+1);
  number_len = serial_num.length()+1;

  /********* Write Serial no. ***********/
  erase_eeprom_section(SERIAL_LOC);
  Serial.println("writing serial to eeprom..");
  for (uint8_t i = 0; i < number_len; i++)
  {
    Serial.print(the_number[i]);
    if(!eeprom_write_char(byte(SERIAL_LOC+i), byte(the_number[i])))
    {
      Serial.println("\nfailed");
      return 0;
    }
  }
  /********** Update Model no. **********/
  erase_eeprom_section(MODEL_LOC);
  Serial.println("\nupdating model number..");
  char model_num[VER_NUM_LEN+2];
  model_num[0] = 'v';
  model_num[VER_NUM_LEN-1] = '\0';
  for ( uint8_t i = 0; i < VER_NUM_LEN; i++)
  {
    model_num[i+1] = the_number[VER_NUM_INDEX+i];
  }
  for (uint8_t i = 0; i < VER_NUM_LEN+2; i++)
  {
    if(!eeprom_write_char(byte(MODEL_LOC+i), byte(model_num[i])))
    {
      Serial.println("failed");
      return 0;
    }
  }
  Serial.println("done!");
  return true;
}

bool read_from_eeprom()
{
  uint8_t addr;
  String ser_num;
  String model_num;
  /********* Serial number **********/
  addr = SERIAL_LOC;
  while (eeprom_read_char(addr) != '\0')
  {
    ser_num += eeprom_read_char(addr++);
  }

  /********* Model number ***********/
  addr = MODEL_LOC;
  while (eeprom_read_char(addr) != '\0')
  {
    model_num += eeprom_read_char(addr++);
  }
  Serial.print("Serial:"); Serial.println(ser_num);
  Serial.print("Model:"); Serial.println(model_num);
  return true;
}

bool erase_eeprom_section(uint8_t addr)
{
  for (uint8_t i = 0; i < MAX_IN_NUM_LEN+BUFFER_BYTES; i++)
  {
    if(!eeprom_write_char(byte(addr+i), byte(0xff)))  // erasing = writing 0xffh
    {
      Serial.println("\nfailed to erase section");
      return 0;
    }
  }
  return true;
}

void read_input()
{
  if (Serial.available())
  {
    String mode = Serial.readStringUntil('\n');
    mode.trim();
    mode.toLowerCase();
    if (mode == "w")
    {
      options_onscreen = false;
      while (Serial.available())
      {
        Serial.read();  // empty out the buffer for serial number
      }

      Serial.println("Enter the number");
      while (!Serial.available());
      String in_string = Serial.readStringUntil('\n');
      // Scanned/Input number is going to end with '\r\n'
      in_string.trim();
      write_to_eeprom(in_string);
    }
    else if (mode == "r")
    {
      options_onscreen = false;
      read_from_eeprom();
    }
    else
    {
      Serial.println("Invalid input");
      options_onscreen = false;
    }
  }
}

void send_ack()
{
  Serial.println("ok");
  Serial.println("ok");
}

void show_options()
{
  if (!options_onscreen)
  {
    Serial.println("--------------------------");
    Serial.println("\nSerial W(rite) or R(ead)?");
    options_onscreen = true;
  }
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
}

void loop()
{
  show_options();
  read_input();
}
