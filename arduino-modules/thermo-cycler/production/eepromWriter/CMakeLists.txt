include(Arduino)

add_arduino_command(
  SKETCH "${CMAKE_CURRENT_SOURCE_DIR}/eepromWriter.ino"
  FQBN "Opentrons:samd:thermocycler_m0"
  TARGET_NAME thermocycler-eeprom-writer
  OUTPUT_FORMAT "bin"
  )

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../../eepromWriter.ino.hex"
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.hex
  COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.hex"
          "${CMAKE_CURRENT_BINARY_DIR}/../../eepromWriter.ino.hex"
  VERBATIM
)
