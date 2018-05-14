//eepromWriter.ino
#include <Arduino.h>
#include <EEPROM.h>
#include "gcode.h"

// Sample EEPROM representation after being written to:
// 0x00 	0 1 2 3 4 5 6 7 8 9 A B C \0 \0 \0	Serial Number
// 0x10 	a b c d e f 1 2 3 \0 \0 \0 \0 \0 \0	Model number
// 0x20 	1 a 2 b 3 c 4 d 5 e 6 f 7 8 9 0 	16-bit crc

// Assuming both ID & Model no. are not more than 16 bytes long
#define DEVICE_SERIAL_ADDR		0x00
#define DEVICE_MODEL_ADDR	0x10
#define CRC_ADDR			0x20

#define END_OF_LINE			'#'

Gcode gcode = Gcode();

int red_led = 5;
bool data_valid = false;

unsigned long model_serial_crc(){

	const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for(int index = 0; index < CRC_ADDR; ++index){
  	crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
  	crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
  	crc = ~crc;
  }

  return crc;
}

bool check_data_validity(){
	unsigned long current_crc = model_serial_crc();
  	unsigned long stored_crc = 0;
  	EEPROM.get(CRC_ADDR, stored_crc);

  	if(current_crc == stored_crc)
  		return true;
  	return false;
}

void setup() {

  pinMode(red_led, OUTPUT);
  gcode.setup(115200);

  data_valid = check_data_validity();
  if(!data_valid){

  	// Clear EEPROM
  	for(int i = 0; i < EEPROM.length(); i++)
  		EEPROM.write(i,0);
  }
  // (Edge case- what about the chance of a freaky coincidence when
  // the data is invalid but the value stored at 
  // CRC_ADDR actually matches the CRC)
  
  Serial.begin(115200);
  while(!Serial);

}

void send_error(int error_code){
	switch(error_code){
		case 0:
		 	Serial.println("Data in EEPROM is invalid");
		 	break;
		case 1:
			Serial.println("Error writing to EEPROM");
			break;
		default:
			break;
	}
}

void send_ack(){
	Serial.print("ok\r\nok\r\n");
}

void send_serial_num(){
	char serial[16] = {'\0'};
	if(!data_valid){
		send_error(0);
		return;
	}
	for(byte i = 0; i < 16; ++i){
		EEPROM.get(DEVICE_SERIAL_ADDR + i, serial[i]);
		if(serial[i] == '\0')
			break;

		Serial.println(serial[i]);
		send_ack();
	}
}

void write_serial_num(){
	String serial_num = parse_serial_num();
	for(byte i = 0; i < 16; ++i){
		if(i >= serial_num.length())
			EEPROM.put(DEVICE_SERIAL_ADDR + i, '\0');
		else
			EEPROM.put(DEVICE_SERIAL_ADDR + i, serial_num.charAt(i));
	}
}

String parse_serial_num(){

}

void read_model(){

}

void write_model(){

}

void update_crc(){

}

void read_gcode(){
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      if (gcode.code == GCODE_READ_DEVICE_SERIAL){
          send_serial_num();
      }
      else if (gcode.code == GCODE_WRITE_DEVICE_SERIAL){
          write_serial_num();
          update_crc();
      }
      else if (gcode.code == GCODE_READ_DEVICE_MODEL){
          read_model();
      }
      else if (gcode.code == GCODE_WRITE_DEVICE_MODEL){
          write_model();
          update_crc();
      }
      }
    send_ack();
	}
}
void loop() {
  // Wait for Serial input

  read_gcode();
  // If Serial input has 'M369' && data_valid
  // Read while databyte != '#'
  	// char_array.Append EEPROM.read(DEVICE_ID_ADDR)

  // If Serial input has 'M370'
  // Write from byte array to EEPROM.write(DEVICE_ID_ADDR,byte[i])
  // Update CRC at CRC_ADDR

  // If Serial input has 'M371' && data_valid
  // Read while databyte != '#'
  	// char_array.Append EEPROM.read(DEVICE_MODEL_ADDR)

  // If Serial input has 'M372'
  // Write from byte array to EEPROM.write(DEVICE_MODEL_ADDR,byte[i])
  // Update CRC at CRC_ADDR
}


