project("MagneticModule"
  LANGUAGES CXX
  DESCRIPTION "Firmware for the Opentrons Magnetic Module")

include(VersionForProject)
include(Arduino)
version_for_project("magdeck")

set_source_files_properties(
  "./mag-deck-arduino/mag-deck-arduino.ino"
  PROPERTIES
  MD_FW_VERSION ${magdeck_VERSION}
  ARDUINO_DEFINES "MD_FW_VERSION")

add_arduino_command(
  OUTPUT_NAME magdeck
  SKETCH "${CMAKE_CURRENT_LIST_DIR}/mag-deck-arduino/mag-deck-arduino.ino"
  FQBN "Opentrons:avr:tempdeck32u4cat"
  PROJECT_FILES "${CMAKE_CURRENT_LIST_DIR}/mag-deck-arduino/gcodemagdeck.cpp"
                "${CMAKE_CURRENT_LIST_DIR}/mag-deck-arduino/gcodemagdeck.h"
                "${CMAKE_CURRENT_LIST_DIR}/mag-deck-arduino/magdeck.h"
                "${CMAKE_CURRENT_LIST_DIR}/memory.cpp"
                "${CMAKE_CURRENT_LIST_DIR}/memory.h"
                "${CMAKE_CURRENT_LIST_DIR}/motion.h")
