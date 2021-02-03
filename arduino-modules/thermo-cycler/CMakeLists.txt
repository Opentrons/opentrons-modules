include(VersionForProject)
version_for_project("thermocycler")

add_subdirectory("thermo-cycler-arduino")
add_subdirectory("production/eepromWriter")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/firmware_uploader.py"
         "${CMAKE_CURRENT_BINARY_DIR}/serial_and_firmware_uploader.py"
         "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino.ino.bin"
         "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.bin"
  COMMAND ${CMAKE_COMMAND} -E copy
         "${CMAKE_CURRENT_SOURCE_DIR}/production/firmware_uploader.py"
         "${CMAKE_CURRENT_BINARY_DIR}/firmware_uploader.py"
  COMMAND ${CMAKE_COMMAND} -E copy
         "${CMAKE_CURRENT_SOURCE_DIR}/production/serial_and_firmware_uploader.py"
         "${CMAKE_CURRENT_BINARY_DIR}/serial_and_firmware_uploader.py"
  COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino/thermo-cycler-arduino.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino.ino.bin"
  COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/production/eepromWriter/eepromWriter.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.bin"
  DEPENDS thermocycler-firmware thermocycler-eeprom-writer
  VERBATIM
)

add_custom_target(thermocycler ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/firmware_uploader.py"
          "${CMAKE_CURRENT_BINARY_DIR}/serial_and_firmware_uploader.py"
  )

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/thermocycler-${thermocycler_VERSION}.zip"
  DEPENDS thermocycler
  COMMAND ${CMAKE_COMMAND} -E tar c
          "${CMAKE_BINARY_DIR}/thermocycler-${thermocycler_VERSION}.zip"
          --format=zip
          --
          "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/firmware_uploader.py"
          "${CMAKE_CURRENT_BINARY_DIR}/serial_and_firmware_uploader.py"
  VERBATIM
  )

add_custom_target(thermocycler-zip
  DEPENDS "${CMAKE_BINARY_DIR}/thermocycler-${thermocycler_VERSION}.zip")

install(FILES
  "${CMAKE_BINARY_DIR}/thermocycler-${thermocycler_VERSION}.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/thermo-cycler-arduino.ino.bin"
  "${CMAKE_CURRENT_BINARY_DIR}/eepromWriter.ino.bin"
  "${CMAKE_CURRENT_BINARY_DIR}/firmware_uploader.py"
  "${CMAKE_CURRENT_BINARY_DIR}/serial_and_firmware_uploader.py"
  DESTINATION "thermocycler"
)
