include(VersionForProject)
include(Arduino)
version_for_project("magdeck")

set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/mag-deck-arduino.ino"
  PROPERTIES
  MD_FW_VERSION ${magdeck_VERSION}
  ARDUINO_DEFINES "MD_FW_VERSION")

add_arduino_command(
  SKETCH "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/mag-deck-arduino.ino"
  FQBN "Opentrons:avr:magdeck32u4cat"
  TARGET_NAME magdeck-firmware
  OUTPUT_FORMAT "hex"
  PROJECT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/gcodemagdeck.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/gcodemagdeck.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/magdeck.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/memory.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/memory.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/mag-deck-arduino/motion.h")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
         "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
         "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/eepromWriter.hex" "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/avrdude.conf" "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/../eepromWriter/write_module_memory.py" "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  VERBATIM
)

add_custom_target(magdeck ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
          "${CMAKE_CURRENT_BINARY_DIR}/mag-deck-arduino.ino.hex"
)

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/magdeck-${magdeck_VERSION}.zip"
  DEPENDS magdeck
  COMMAND ${CMAKE_COMMAND} -E tar c
          "${CMAKE_BINARY_DIR}/magdeck-${magdeck_VERSION}.zip"
          --format=zip
          --
          "${CMAKE_CURRENT_BINARY_DIR}/mag-deck-arduino.ino.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
)

add_custom_target(magdeck-zip
  DEPENDS "${CMAKE_BINARY_DIR}/magdeck-${magdeck_VERSION}.zip"
)

install(FILES
  "${CMAKE_BINARY_DIR}/magdeck-${magdeck_VERSION}.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/mag-deck-arduino.ino.hex"
  "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.hex"
  "${CMAKE_CURRENT_BINARY_DIR}/avrdude.conf"
  "${CMAKE_CURRENT_BINARY_DIR}/write_module_memory.py"
  DESTINATION "magdeck"
)
