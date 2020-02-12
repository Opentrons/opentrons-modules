#include "gcode.h"
#include <cerrno>
#include <cstdlib>

#undef GCODE_DEF
#define GCODE_DEF(_, str) #str

const String COMMAND_CODES[] = { GCODES_TABLE };
const char _CHARACTERS_TO_STRIP[] = {' ', '\r', '\n'};

GcodeHandler::GcodeHandler() {}

void GcodeHandler::_strip_serial_buffer()
{
  char temp[_serial_buffer_string.length()];
  char * p;
  p = temp;
  for (const auto& sb_char : _serial_buffer_string)
  {
    bool skip = false;
    for (const auto& c: _CHARACTERS_TO_STRIP)
    {
      if (sb_char == c)
      {
        skip = true;
      }
    }
    if (!skip)
    {
      *p = sb_char;
      p++;
    }
  }
  *p = '\0';
  _serial_buffer_string = temp;
}

/* Searches for gcode in the given string and updates the index reference with
 * index of gcode found */
bool GcodeHandler::_find_command(const String& _string, uint8_t * code_int, uint8_t * index)
{
  for (uint8_t str_index = 0; str_index < _string.length(); str_index++, (*index)++)
  {
    // Search for substring that matches a valid thermocycler gcode
    for (uint8_t i= CODE_INT(Gcode::no_code) + 1; i < CODE_INT(Gcode::max); i++)
    {
      if (_string.substring(str_index, str_index+COMMAND_CODES[i].length()) == COMMAND_CODES[i])
      {
        // A gcode found
        *code_int = i;
        return true;
      }
    }
  }
  return false;
}

/* get_command() returns the first command in the _serial_buffer_string and
 * stores the code and its args into _command struct  */
Gcode GcodeHandler::get_command()
{
  _strip_serial_buffer();
  _command.code = Gcode::no_code;
  uint8_t arg_start = 0;
  uint8_t sbuf_index = 0;
  uint8_t code_int = 0;
  while(sbuf_index < _serial_buffer_string.length())
  {
    if(_find_command(_serial_buffer_string.substring(sbuf_index), &code_int, &sbuf_index))
    {
      if(_command.code != Gcode::no_code)
      {
        // Second Gcode in a row
        sbuf_index--;
        break;
      }
      _command.code = static_cast<Gcode>(code_int);
      arg_start = sbuf_index + COMMAND_CODES[code_int].length();
      sbuf_index = arg_start;
    }
  }
  if(arg_start != 0)
  {
    _command.args_string = _serial_buffer_string.substring(arg_start, sbuf_index+1);
  }
  _serial_buffer_string.remove(0, sbuf_index+1);
  return _command.code;
}

/* Returns true if a valid line ending in '\r\n' is received. This function reads
 * the received line into `_serial_buffer_string`, removing it from the Serial
 * buffer of the uC */
bool GcodeHandler::received_newline()
{
  if (Serial.available())
  {
    if (_serial_buffer_string.length() > MAX_SERIAL_BUFFER_LENGTH)
    {
      _serial_buffer_string = "";
    }
    _serial_buffer_string += Serial.readStringUntil('\n');
    if (_serial_buffer_string.charAt(_serial_buffer_string.length() - 1) == '\r')
    {
      return true;
    }
  }
  return false;
}

/* Returns whether _serial_buffer_string has unread characters (false) or not (true) */
bool GcodeHandler::buffer_empty()
{
  return !_serial_buffer_string.length() > 0;
}

void GcodeHandler::send_ack()
{
  Serial.println("ok");
  Serial.println("ok");
}

/* If the given key and a valid corresponding number is present in _command.args_string,
 * pop_arg(key) pops the argument off of _command.args_string and returns true.
 * Else, returns false  */
bool GcodeHandler::pop_arg(char key)
{
  int key_index = _command.args_string.indexOf(key);
  if (key_index >= 0)
  {
    // Convert to number
    String number_string = _command.args_string.substring(key_index+1);
    uint8_t array_len = number_string.length()+1; // the number + array end char

    const char *p = number_string.c_str();
    char *end;
    float f = strtod(p, &end);
    _command.args_string.remove(key_index, end-p+1);  // +1 cuz key_index has to be included too
    if(p != end && errno != ERANGE)
    {
      _parsed_arg = f;
      return true;
    }
  }
  return false;
}

/* Returns the argument obtained by pop_arg() */
float GcodeHandler::popped_arg()
{
  return _parsed_arg;
}

void GcodeHandler::device_info_response(String serial, String model, String version)
{
  Serial.print(F("serial:"));
  Serial.print(serial);
  Serial.print(F(" model:"));
  Serial.print(model);
  Serial.print(F(" version:"));
  Serial.print(version);
  Serial.println();
}

void GcodeHandler::targetting_temperature_response(float target_temp,
                                    float current_temp, float time_remaining)
{
  Serial.print(F("T:"));
  Serial.print(target_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.print(F(" C:"));
  Serial.print(current_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.print(F(" H:"));
  Serial.println((unsigned int)time_remaining);
}

void GcodeHandler::targetting_temperature_response(float target_temp,
                                                   float current_temp)
{
  Serial.print(F("T:"));
  Serial.print(target_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.print(F(" C:"));
  Serial.println(current_temp, SERIAL_DIGITS_IN_RESPONSE);
}

void GcodeHandler::idle_temperature_response(float current_temp)
{
  Serial.print(F("T:none"));
  Serial.print(F(" C:"));
  Serial.print(current_temp, SERIAL_DIGITS_IN_RESPONSE);
  Serial.println(F(" H:none"));
}

void GcodeHandler::idle_lid_temperature_response(float current_temp)
{
  Serial.print(F("T:none"));
  Serial.print(F(" C:"));
  Serial.println(current_temp, SERIAL_DIGITS_IN_RESPONSE);
}
void GcodeHandler::response(String param, String msg)
{
  Serial.print(param);
  Serial.print(F(":"));
  response(msg);
}

void GcodeHandler::response(String msg)
{
  Serial.println(msg);
}

void GcodeHandler::add_debug_response(String param, float val)
{
  Serial.print(param);
  Serial.print(F(": "));
  Serial.print(val, 4);
  Serial.print(F("\t"));
}

void GcodeHandler::add_debug_timestamp()
{
  Serial.print(F("millis: "));
  Serial.print(millis());
  Serial.print(F("\t"));
}

/* Use in .ino setup() with the baudrate to enable
 * serial communication using gcodes */
void GcodeHandler::setup(int baudrate)
{
  Serial.begin(baudrate);
  Serial.setTimeout(3);
}
