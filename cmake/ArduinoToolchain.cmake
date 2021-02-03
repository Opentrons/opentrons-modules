# This is a toolchain file to use the arduino IDE headless to build
# firmware. It also downloads the Arduino IDE if it cannot be found.

message(STATUS "Setting arduino toolchain")
include(FetchContent)
set(ARDUINO_IDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../arduino_ide")
message(STATUS "System name ${CMAKE_HOST_SYSTEM_NAME}")
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(ARDUINO_ARCHIVE
    "arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
  set(ARDUINO_ARCHIVE
    "arduino-${ARDUINO_IDE_VERSION}-macosx.zip")
else()
  set(ARDUINO_ARCHIVE
    "arduino-${ARDUINO_IDE_VERSION}-windows.zip")
endif()

message(STATUS ${ARDUINO_ARCHIVE})

FetchContent_Declare(
  ARDUINO_IDE
  PREFIX ${ARDUINO_IDE_DIR}
  SOURCE_DIR "${ARDUINO_IDE_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${ARDUINO_IDE_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "https://downloads.arduino.cc/${ARDUINO_ARCHIVE}"
  )

macro(FIND_ARDUINO REQUIRED)
  find_program(
    ARDUINO_EXECUTABLE
    arduino
    PATH_SUFFIXES Contents/MacOS
    DOC "Location of the Arduino IDE executable"
    PATHS ${ARDUINO_IDE_DIR}
    HINTS ${ARDUINO_IDE_DIR}/${CMAKE_HOST_SYSTEM_NAME}
    ${REQUIRED}
    )
endmacro()

find_arduino("")

if(${ARDUINO_EXECUTABLE} STREQUAL "ARDUINO_EXECUTABLE-NOTFOUND")
  message(STATUS "Didn't find arduino, downloading")
  FetchContent_Populate(ARDUINO_IDE)
  find_arduino("")

  execute_process(
    COMMAND ${ARDUINO_EXECUTABLE} --pref "sketchbook.path=${CMAKE_CURRENT_LIST_DIR}/../arduino-modules" --save-prefs
    COMMAND ${ARDUINO_EXECUTABLE} --pref "boardsmanager.additional.urls=https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json" --save-prefs
    COMMAND ${ARDUINO_EXECUTABLE} --install-boards "Opentrons:samd:${OPENTRONS_SAMD_BOARDS_VERSION}"
    COMMAND ${ARDUINO_EXECUTABLE} --install-boards "arduino:samd:${ARDUINO_SAMD_VERSION}"
    COMMAND ${ARDUINO_EXECUTABLE} --install-boards "Opentrons:avr"
    )

endif()

message(STATUS "current list dir is ${CMAKE_CURRENT_LIST_DIR}")
find_arduino(REQUIRED)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION 0)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
