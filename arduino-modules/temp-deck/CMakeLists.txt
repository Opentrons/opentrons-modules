include(VersionForProject)
include(Arduino)
version_for_project("tempdeck")

set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/temp-deck-arduino.ino"
  PROPERTIES
  TD_FW_VERSION ${tempdeck_VERSION}
  ARDUINO_DEFINES "TD_FW_VERSION")

add_arduino_command(
  SKETCH "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/temp-deck-arduino.ino"
  TARGET_NAME tempdeck-firmware
  FQBN "Opentrons:avr:tempdeck32u4cat"
  OUTPUT_FORMAT "hex"
  PROJECT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/gcode.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/gcode.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/temp-deck.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/lights.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/lights.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/peltiers.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/peltiers.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/thermistor.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/thermistor.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/memory.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/temp-deck-arduino/memory.h")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
         "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
         "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/eepromWriter.hex" "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/avrdude.conf" "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/write_module_memory.py" "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  VERBATIM
)

add_custom_target(tempdeck ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
          "${CMAKE_CURRENT_BINARY_DIR}/temp-deck-arduino.ino.hex")

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/tempdeck-${tempdeck_VERSION}.zip"
  DEPENDS tempdeck
  COMMAND ${CMAKE_COMMAND} -E tar c
          "${CMAKE_BINARY_DIR}/tempdeck-${tempdeck_VERSION}.zip"
          --format=zip
          --
          "${CMAKE_CURRENT_BINARY_DIR}/temp-deck-arduino.ino.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
)

add_custom_target(tempdeck-zip
  DEPENDS "${CMAKE_BINARY_DIR}/tempdeck-${tempdeck_VERSION}.zip"
)

install(FILES
  "${CMAKE_BINARY_DIR}/tempdeck-${tempdeck_VERSION}.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/temp-deck-arduino.ino.hex"
  "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
  "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
  "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  DESTINATION "tempdeck"
)
