#include "gcode.h"

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
  _strip_serial_buffer();
  GCODE_BUFFER_STRING += SERIAL_BUFFER_STRING;
  SERIAL_BUFFER_STRING = "";
  code = GCODE_NO_CODE;
  while (GCODE_BUFFER_STRING.length()) {
    for (uint8_t i=GCODE_GET_LID_STATUS; i<=TOTAL_GCODE_COMMAND_CODES; i++) {
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
    SERIAL_BUFFER_STRING += Serial.readStringUntil('\n');
    if (SERIAL_BUFFER_STRING.charAt(SERIAL_BUFFER_STRING.length() - 1) == '\r') {
      return true;
    }
  }
  return false;
}

void Gcode::send_ack() {
  Serial.println("ok");
  Serial.println("ok");
}

bool Gcode::read_number(char key) {
  // NOTE: Has a flaw where if there's 2 `set_temperature` gcodes in the buffer,
  //       with the first one without a hold time and the next having one,
  //       then the next hold time will be captured and associated with the
  //       first gcode
  int starting_key_index = GCODE_BUFFER_STRING.indexOf(key);
  if (starting_key_index >= 0) {
    String number_string = "";
    char next_char;
    bool decimal = false;
    while (starting_key_index + 1 < GCODE_BUFFER_STRING.length()) {
      starting_key_index++;
      next_char = GCODE_BUFFER_STRING.charAt(starting_key_index);
      if (isDigit(next_char)) {
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
  }
  return false;
}

void Gcode::device_info_response(String serial, String model, String version) {
  Serial.print(F("serial:"));
  Serial.print(serial);
  Serial.print(F(" model:"));
  Serial.print(model);
  Serial.print(F(" version:"));
  Serial.print(version);
  Serial.println();
}

void Gcode::targetting_temperature_response(float target_temp, float current_temp,\
   float time_remaining) {
  Serial.print(F("T:"));
  Serial.print(target_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.print(F(" C:"));
  Serial.print(current_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.print(F(" H:"));
  Serial.println(time_remaining);
}

void Gcode::idle_temperature_response(float current_temp) {
  Serial.print(F("T: none"));
  Serial.print(F(" C:"));
  Serial.print(current_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.println(F(" H: none"));
}

void Gcode::response(String param, String msg) {
  Serial.print(param);
  Serial.print(": ");
  response(msg);
}

void Gcode::response(String msg) {
  Serial.println(msg);
}

void Gcode::setup(int baudrate) {
  COMMAND_CODES[GCODE_GET_LID_STATUS] =  "M119";
  COMMAND_CODES[GCODE_OPEN_LID] =        "M126";
  COMMAND_CODES[GCODE_CLOSE_LID] =       "M127";
  COMMAND_CODES[GCODE_SET_LID_TEMP] =    "M140";
  COMMAND_CODES[GCODE_DEACTIVATE_LID_HEATING] = "M108";
  COMMAND_CODES[GCODE_SET_PLATE_TEMP] =  "M104";
  COMMAND_CODES[GCODE_GET_PLATE_TEMP] =  "M105";
  COMMAND_CODES[GCODE_SET_RAMP_RATE] =   "M566";
  COMMAND_CODES[GCODE_EDIT_PID_PARAMS] = "M301";
  COMMAND_CODES[GCODE_PAUSE] =           "M76";
  COMMAND_CODES[GCODE_DEACTIVATE_ALL] =  "M18";
  COMMAND_CODES[GCODE_DFU] =             "dfu";
  Serial.begin(baudrate);
  Serial.setTimeout(3);
}
