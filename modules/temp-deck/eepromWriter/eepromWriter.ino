// eepromWriter.ino
// Known Bug: If a string like "M370 serial123 M12" is passed,
// i.e., a write gcode with an *invalid* gcode after the write parameter,
// the parameter will get interpreted as 'serial123M12' 
// A string with a valid gcode would be parsed correctly e.g "M370 serial123 M369"

#include <Arduino.h>
#include <EEPROM.h>
#include "gcode.h"

// Sample EEPROM representation after being written to:
// 0x00 	0 1 2 3 4 5 6 7 8 9 A B C \0 \0 \0...		Serial Number
// 0x20 	t e m p - v 1 . 3 . 4 \0 \0 \0 \0 \0...		Model number
// 0x40 	1 a 2 b 3 c 4 d 5 e 6 f 7 8 9 0...	 		32-bit crc (serial)
// 0x60 	a b c d e f 1 2 3 \0 \0 \0 \0 \0 \0...		32-bit crc (model)

// Assuming both ID & Model no. are not more than 16 bytes long
#define DEVICE_SERIAL_ADDR		0x00
#define DEVICE_MODEL_ADDR		0x20
#define SERIAL_CRC_ADDR			0x40
#define MODEL_CRC_ADDR			0x60
#define DATA_MAX_LENGTH			32

Gcode gcode = Gcode();

int red_led = 5;

const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

enum IDs{
	SERIAL_NUM,
	MODEL_NUM
};

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

unsigned long calculate_crc(uint32_t address){

	unsigned long crc = ~0L;

	for(int index = 0; index < address + DATA_MAX_LENGTH; ++index){
		crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}

	return crc;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool check_eeprom_validity(int ID_type){
	unsigned long current_crc = 0;
	unsigned long stored_crc = 0;

	switch(ID_type){
		case SERIAL_NUM:
			current_crc = calculate_crc(DEVICE_SERIAL_ADDR);
			EEPROM.get(SERIAL_CRC_ADDR, stored_crc);
			break;
		case MODEL_NUM:
			current_crc = calculate_crc(DEVICE_MODEL_ADDR);
			EEPROM.get(MODEL_CRC_ADDR, stored_crc);
			break;
		default:
			break;
	}
	if(current_crc == stored_crc)
		return true;
	return false;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void send_error(int error_code){
	switch(error_code){
		case 0:
			Serial.println("Data in EEPROM is invalid");
		break;
		case 1:
			Serial.println("Error in gcode/parameter");
		break;
		case 2:
			Serial.println("Error writing to EEPROM");
		default:
		break;
	}
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_from_eeprom(int ID_type){
	char eeprom_data[DATA_MAX_LENGTH] = {'\0'};

	if(!check_eeprom_validity(ID_type)){
		send_error(0);
		return;
	}

	uint32_t address;

	switch(ID_type){
		case SERIAL_NUM:
			address = DEVICE_SERIAL_ADDR;
			Serial.print("Serial: ");
			break;
		case MODEL_NUM:
			address = DEVICE_MODEL_ADDR;
			Serial.print("Model: ");
			break;
		default:
			break;
	}

	for(byte i = 0; i < DATA_MAX_LENGTH; ++i){
		EEPROM.get(address + i, eeprom_data[i]);
		if(eeprom_data[i] == '\0')
		break;

		Serial.print(eeprom_data[i]);
	}
	Serial.println();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void write_to_eeprom(uint32_t address){
	String* parameter_string = gcode.read_parameter();

	if(parameter_string->length() == 0 || 
		parameter_string->length() > DATA_MAX_LENGTH){
		send_error(1);
		return;
	}

	digitalWrite(red_led, HIGH);

	for(uint8_t i = 0; i < DATA_MAX_LENGTH; ++i){
		if(i >= parameter_string->length())
			EEPROM.put(address + i, '\0');
		else
			EEPROM.put(address + i, parameter_string->charAt(i));
	}
	delay(500);
	digitalWrite(red_led, LOW);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_serial_crc(){
	unsigned long current_crc = calculate_crc(DEVICE_SERIAL_ADDR);
	EEPROM.put(SERIAL_CRC_ADDR, current_crc);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_model_crc(){
	unsigned long current_crc = calculate_crc(DEVICE_MODEL_ADDR);
	EEPROM.put(MODEL_CRC_ADDR, current_crc);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_gcode(){
	if (gcode.received_newline()) {
		while (gcode.pop_command()) {
			if (gcode.code == GCODE_READ_DEVICE_SERIAL){
				read_from_eeprom(SERIAL_NUM);
			}
			else if (gcode.code == GCODE_WRITE_DEVICE_SERIAL){
				write_to_eeprom(DEVICE_SERIAL_ADDR);
				update_serial_crc();
			}
			else if (gcode.code == GCODE_READ_DEVICE_MODEL){
				read_from_eeprom(MODEL_NUM);
			}
			else if (gcode.code == GCODE_WRITE_DEVICE_MODEL){
				write_to_eeprom(DEVICE_MODEL_ADDR);
				update_model_crc();
			}
		}
		gcode.send_ack();
	}
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

	pinMode(red_led, OUTPUT);
	gcode.setup(115200);
	//Serial.begin(115200);
  	while(!Serial);

	if(!check_eeprom_validity(SERIAL_NUM)){

		Serial.println("EEPROM serial data invalid");	
	  	// Clear EEPROM
	  	for(int i = DEVICE_SERIAL_ADDR; i < DEVICE_SERIAL_ADDR + DATA_MAX_LENGTH; i++)
	  	EEPROM.write(i,0);
  	}

  	if(!check_eeprom_validity(MODEL_NUM)){

		Serial.println("EEPROM model data invalid");	
	  	// Clear EEPROM
	  	for(int i = DEVICE_MODEL_ADDR; i < DEVICE_MODEL_ADDR + DATA_MAX_LENGTH; i++)
	  	EEPROM.write(i,0);
  	}
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop() {
  // Wait for Serial input
  read_gcode();
}


