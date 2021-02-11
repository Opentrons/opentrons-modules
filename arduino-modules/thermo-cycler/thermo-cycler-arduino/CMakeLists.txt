include(Arduino)

include(VersionForProject)
version_for_project("thermocycler")


set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/thermo-cycler-arduino.ino"
  PROPERTIES
  TC_FW_VERSION ${thermocycler_VERSION}
  ARDUINO_DEFINES "TC_FW_VERSION;LID_WARNING;HFQ_PWM;HW_VERSION;LID_TESTING;RGBW_NEO;VOLUME_DEPENDENCY"
  LID_WARNING ${TC_LID_WARNING}
  HFQ_PWM ${TC_HFQ_PWM}
  HW_VERSION ${TC_HW_VERSION}
  LID_TESTING ${TC_LID_TESTING}
  RGBW_NEO ${TC_RGBW_NEO}
  VOLUME_DEPENDENCY ${TC_VOLUME_DEPENDENCY}
  )

add_arduino_command(
  SKETCH "${CMAKE_CURRENT_SOURCE_DIR}/thermo-cycler-arduino.ino"
  FQBN "Opentrons:samd:thermocycler_m0"
  TARGET_NAME thermocycler-firmware
  OUTPUT_FORMAT "bin"
  PROJECT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/eeprom.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/eeprom.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/fan.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/fan.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/gcode.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/gcode.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/high_frequency_pwm.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/high_frequency_pwm.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/lid.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/lid.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/lights.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/lights.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/peltiers.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/peltiers.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/tc_timer.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/tc_timer.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/thermistorsadc.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/thermistorsadc.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/thermo-cycler.h"
)
