#include "Gcode.h"

Gcode::Gcode() {
    Serial.begin(115200);
    Serial.setTimeout(2);
}

void Gcode::_send_ack(){
    Serial.println("ok");
    Serial.println("ok");
}

void Gcode::_strip_serial_buffer(){
  for(uint8_t i=0;i<SERIAL_BUFFER_STRING.length();i++) {
    for (uint8_t c=0;c<sizeof(CHARACTERS_TO_STRIP);c++) {
      if(SERIAL_BUFFER_STRING[i] == CHARACTERS_TO_STRIP[c]){
        SERIAL_BUFFER_STRING.remove(i, 1);
        i--;
        break;
      }
    }
  }
}

void Gcode::_parse_temp_from_buffer() {
  int starting_s_index = SERIAL_BUFFER_STRING.indexOf('S');
  if (starting_s_index >= 0) {
    String parsed_number = "";
    char next_char;
    boolean valid_command = false;
    for (uint8_t i=1;i<4;i++){
      if (SERIAL_BUFFER_STRING.length() <= starting_s_index + i){
        break;
      }
      next_char = SERIAL_BUFFER_STRING.charAt(starting_s_index + i);
      if (next_char >= '0' && next_char <= '9') {
        if (i == 3) {
          valid_command = false;  // number sent is 3 digits, so ignore it
          break;
        }
        parsed_number += next_char;
        valid_command = true;
      }
      else {
        break;
      }
    }
    if (valid_command) {
      target_temp = parsed_number.toInt();
    }
  }
}

void Gcode::_print_current_temperature() {
  Serial.print("T:");
  if (target_temp == NO_TARGET_SET) {
    Serial.print("none");
  }
  else {
    Serial.print(target_temp);
  }
  Serial.print(" C:");
  Serial.println(current_temp);
}

void Gcode::_print_model_and_version() {
  Serial.println("model:temp-v1 version:edge-1a2b3c4");
}

void Gcode::_parse_gcode_commands() {
  _strip_serial_buffer();
  while (SERIAL_BUFFER_STRING.length()) {
    for (uint8_t i=0;i<TOTAL_GCODE_COMMAND_CODES;i++){
      if (SERIAL_BUFFER_STRING.substring(0, COMMAND_CODES[i].length()) == COMMAND_CODES[i]) {
        switch (i) {
          case 0:  // M105  (get temp)
            _print_current_temperature();
            break;
          case 1:  // M104  (set temp)
            _parse_temp_from_buffer();
            break;
          case 2:  // M18  (OFF)
            target_temp = NO_TARGET_SET;
            break;
          case 3:  // M115  (get model/version)
            _print_model_and_version();
            break;
          case 4:  // dfu  (start bootloader)
            Serial.println("Entering bootloader");
            break;
        }
      }
    }
    SERIAL_BUFFER_STRING.remove(0, 1);
  }
  SERIAL_BUFFER_STRING = "";
}

uint8_t Gcode::update(){
  if (Serial.available()){
    SERIAL_BUFFER_STRING += Serial.readStringUntil("\r\n");
    if (SERIAL_BUFFER_STRING.charAt(SERIAL_BUFFER_STRING.length() - 2) == '\r') {
      if (SERIAL_BUFFER_STRING.charAt(SERIAL_BUFFER_STRING.length() - 1) == '\n') {
        _parse_gcode_commands();
        _send_ack();
      }
    }
  }
}
