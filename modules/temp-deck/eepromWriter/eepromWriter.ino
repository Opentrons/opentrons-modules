#include <Arduino.h>

#include "memory.h"

Memory memory = Memory();

String serial = "";
String model = "";

bool reading_in_serial = true;

void print_data() {
  memory.read_serial();
  memory.read_model();
  Serial.print(memory.serial);
  Serial.print(":");
  Serial.print(memory.model);
  Serial.println();
}

void clear_port() {
  while (Serial.available() > 0) {
    Serial.read();
    delay(2);
  }
}

void setup() {
	Serial.begin(115200);
	Serial.setTimeout(30);
  clear_port();
}

void loop() {
	if (Serial.available() > 0) {
    serial = "";
    model = "";
		reading_in_serial = true;
		char val;
		while (Serial.available() > 0) {
      delay(2);
			val = Serial.read();
      if (val == '&') {
        print_data();
        clear_port();
        return;
      }
			else if (val == ':'){
				reading_in_serial = false;
			}
			else if (val == '\r' || val == '\n') {
				// ignore newline and return characters
			}
			else if (reading_in_serial) {
				serial += val;
			}
			else {
				model += val;
			}
		}
    if (serial.length() && model.length()) {
  		memory.writeData = serial;
  		memory.write_serial();
  		memory.writeData = model;
  		memory.write_model();
    }
    Serial.println();  // acknowledgement
	}
}
