#include "Gcode.h"

Gcode::Gcode() {
}

void Gcode::_strip_serial_buffer() {
  for(uint8_t i=0;i<SERIAL_BUFFER_STRING.length();i++) {
    for (uint8_t c=0;c<sizeof(CHARACTERS_TO_STRIP);c++) {
      if(SERIAL_BUFFER_STRING[i] == CHARACTERS_TO_STRIP[c]) {
        SERIAL_BUFFER_STRING.remove(i, 1);
        i--;
        break;
      }
    }
  }
}

bool Gcode::pop_command() {
  code = GCODE_NO_CODE;
  while (GCODE_BUFFER_STRING.length()) {
    for (uint8_t i=0;i<TOTAL_GCODE_COMMAND_CODES;i++) {
      if (GCODE_BUFFER_STRING.substring(0, COMMAND_CODES[i].length()) == COMMAND_CODES[i]) {
        GCODE_BUFFER_STRING.remove(0, COMMAND_CODES[i].length());
        code = i;
        return true;
      }
    }
    GCODE_BUFFER_STRING.remove(0, 1);
  }
  return false;
}

bool Gcode::received_newline() {
  if (Serial.available()){
    if (SERIAL_BUFFER_STRING.length() > MAX_SERIAL_BUFFER_LENGTH) {
      SERIAL_BUFFER_STRING = "";
    }
    SERIAL_BUFFER_STRING += Serial.readStringUntil("\r\n");
    if (SERIAL_BUFFER_STRING.charAt(SERIAL_BUFFER_STRING.length() - 2) == '\r') {
      if (SERIAL_BUFFER_STRING.charAt(SERIAL_BUFFER_STRING.length() - 1) == '\n') {
        _strip_serial_buffer();
        GCODE_BUFFER_STRING += SERIAL_BUFFER_STRING;
        SERIAL_BUFFER_STRING = "";
        return true;
      }
    }
  }
  return false;
}

void Gcode::send_ack() {
  Serial.println("ok");
  Serial.println("ok");
}

bool Gcode::read_number(char key) {
  int starting_key_index = GCODE_BUFFER_STRING.indexOf(key);
  if (starting_key_index >= 0) {
    String number_string = "";
    char next_char;
    boolean valid_command = false;
    bool decimal = false;
    for (uint8_t i=1;i<4;i++) {
      if (GCODE_BUFFER_STRING.length() <= starting_key_index + i) {
        break;
      }
      next_char = GCODE_BUFFER_STRING.charAt(starting_key_index + i);
      if (next_char >= '0' && next_char <= '9') {
        number_string += next_char;
      }
      else if(next_char == '-' && number_string.length() == 0) {
        number_string += next_char;
      }
      else if(next_char == '.' && !decimal && number_string.length() > 0) {
        decimal = true;
        number_string += next_char;
      }
      else {
        break;
      }
    }
    if (number_string) {
      parsed_number = number_string.toFloat();
      return true;
    }
    else {
      return false;
    }
  }
}

void Gcode::print_device_info(String serial, String model, String version) {
  Serial.print("serial:");
  Serial.print(serial);
  Serial.print(" model:");
  Serial.print(model);
  Serial.print(" version:");
  Serial.print(version);
  Serial.println();
}

void Gcode::print_targetting_temperature(int target_temp, int current_temp) {
  Serial.print("T:");
  Serial.print(target_temp);
  Serial.print(" C:");
  Serial.println(current_temp);
}

void Gcode::print_stablizing_temperature(int current_temp) {
  Serial.print("T:");
  Serial.print("none");
  Serial.print(" C:");
  Serial.println(current_temp);
}

void Gcode::print_warning(String msg) {
  Serial.println(msg);
}

void Gcode::setup(int baudrate) {
  COMMAND_CODES[GCODE_GET_TEMP] =     "M105";
  COMMAND_CODES[GCODE_SET_TEMP] =     "M104";
  COMMAND_CODES[GCODE_DEVICE_INFO] =  "M115";
  COMMAND_CODES[GCODE_DISENGAGE] =    "M18";
  COMMAND_CODES[GCODE_DFU] =          "dfu";
  Serial.begin(baudrate);
  Serial.setTimeout(30);
}
