//eepromWriter.ino
#include <Arduino.h>
#include <EEPROM.h>
#include "gcode.h"

// Sample EEPROM representation after being written to:
// 0x00 	0 1 2 3 4 5 6 7 8 9 A B C \0 \0 \0		Serial Number
// 0x10 	a b c d e f 1 2 3 \0 \0 \0 \0 \0 \0		Model number
// 0x20 	1 a 2 b 3 c 4 d 5 e 6 f 7 8 9 0 		32-bit crc (serial)
// 0x30 	t e m p - v 1 . 3 . 4 \0 \0 \0 \0 \0	32-bit crc (model)

// Assuming both ID & Model no. are not more than 16 bytes long
#define DEVICE_SERIAL_ADDR		0x00
#define DEVICE_MODEL_ADDR		0x10
#define SERIAL_CRC_ADDR			0x20
#define MODEL_CRC_ADDR			0x30

Gcode gcode = Gcode();

int red_led = 5;
bool serial_data_valid = false;
bool model_data_valid = false;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

unsigned long serial_crc(){

	const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned long crc = ~0L;

	for(int index = 0; index < DEVICE_SERIAL_ADDR + 16; ++index){
		crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}

	return crc;
}

unsigned long model_crc(){

	const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned long crc = ~0L;

	for(int index = 0; index < DEVICE_MODEL_ADDR + 16; ++index){
		crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}

	return crc;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool check_serial_validity(){
	unsigned long current_crc = serial_crc();
	unsigned long stored_crc = 0;
	EEPROM.get(SERIAL_CRC_ADDR, stored_crc);

	if(current_crc == stored_crc)
		return true;
	return false;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool check_model_validity(){
	unsigned long current_crc = model_crc();
	unsigned long stored_crc = 0;
	EEPROM.get(MODEL_CRC_ADDR, stored_crc);

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

void read_serial_ID(){
	char serial[16] = {'\0'};
	if(!check_serial_validity()){
		send_error(0);
		return;
	}

	Serial.print("Serial: ");
	for(byte i = 0; i < 16; ++i){
		EEPROM.get(DEVICE_SERIAL_ADDR + i, serial[i]);
		if(serial[i] == '\0')
		break;

		Serial.print(serial[i]);
	}
	Serial.println();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void write_serial_ID(){
	
	String* serial_num = gcode.read_parameter();
	if(serial_num->length() == 0){
		send_error(1);
		return;
	}

	digitalWrite(red_led, HIGH);

	for(byte i = 0; i < 16; ++i){
		if(i >= serial_num->length())
			EEPROM.put(DEVICE_SERIAL_ADDR + i, '\0');
		else
			EEPROM.put(DEVICE_SERIAL_ADDR + i, serial_num->charAt(i));
	}
	delay(500);
	digitalWrite(red_led, LOW);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_model(){
	char model[16] = {'\0'};
	if(!check_model_validity()){
		send_error(0);
		return;
	}

	Serial.print("Model: ");
	for(byte i = 0; i < 16; ++i){
		EEPROM.get(DEVICE_MODEL_ADDR + i, model[i]);
		if(model[i] == '\0')
		break;

		Serial.print(model[i]);
	}
	Serial.println();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void write_model(){

	String* model_num = gcode.read_parameter();
	digitalWrite(red_led, HIGH);

	if(model_num->length() == 0){
		send_error(1);
		return;
	}

	for(byte i = 0; i < 16; ++i){
		if(i >= model_num->length())
			EEPROM.put(DEVICE_MODEL_ADDR + i, '\0');
		else
			EEPROM.put(DEVICE_MODEL_ADDR + i, model_num->charAt(i));
	}
	delay(500);
	digitalWrite(red_led, LOW);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_serial_crc(){
	unsigned long current_crc = serial_crc();
	EEPROM.put(SERIAL_CRC_ADDR, current_crc);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_model_crc(){
	unsigned long current_crc = model_crc();
	EEPROM.put(MODEL_CRC_ADDR, current_crc);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_gcode(){
	if (gcode.received_newline()) {
		while (gcode.pop_command()) {
			if (gcode.code == GCODE_READ_DEVICE_SERIAL){
				read_serial_ID();
			}
			else if (gcode.code == GCODE_WRITE_DEVICE_SERIAL){
				write_serial_ID();
				update_serial_crc();
			}
			else if (gcode.code == GCODE_READ_DEVICE_MODEL){
				read_model();
			}
			else if (gcode.code == GCODE_WRITE_DEVICE_MODEL){
				write_model();
				update_model_crc();
			}
			gcode.send_ack();
		}
		
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

	if(!check_serial_validity()){

		Serial.println("EEPROM serial data invalid");	//TODO remove
	  	// Clear EEPROM
	  	for(int i = DEVICE_SERIAL_ADDR; i < DEVICE_SERIAL_ADDR + 16; i++)
	  	EEPROM.write(i,0);
  	}

  	if(!check_model_validity()){

		Serial.println("EEPROM model data invalid");	//TODO remove
	  	// Clear EEPROM
	  	for(int i = DEVICE_MODEL_ADDR; i < DEVICE_MODEL_ADDR + 16; i++)
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


