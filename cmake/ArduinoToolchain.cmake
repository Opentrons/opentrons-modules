#[=======================================================================[.rst:
ArduinoToolchain
----------------

This module should never be imported and used in a CMakeLists.txt file at either
build or configuration time. It is only intended to be used as a toolchain specified
either in a preset or with the ``-DCMAKE_TOOLCHAIN_FILE`` command-line option.

It will download the appropriate arduino IDE for the current system to
``opentrons-modules/arduino-ide/SYSTEM_NAME`` and set cache variables accordingly.

It also configures the IDE with some settings that would be a little annoying to set
every time we build.

Because this all works once and is gated on downloading the IDE again, if you ever
need to redownload it you should ``rm -rf ./arduino_ide`` and then rerun ``cmake``.
#]=======================================================================]


message(STATUS "Setting arduino toolchain")
include(FetchContent)
set(ARDUINO_IDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../arduino_ide")

# Figure out the appropriate archive url tail
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

# Set up the fetch
FetchContent_Declare(
  ARDUINO_IDE
  PREFIX ${ARDUINO_IDE_DIR}
  SOURCE_DIR "${ARDUINO_IDE_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${ARDUINO_IDE_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "https://downloads.arduino.cc/${ARDUINO_ARCHIVE}"
  )

# This macro means we have to type this less, and we'll be calling it a lot
# since we a) want to find the IDE b) want to be OK with not finding it the
# first time, since the point is to download it and c) want to require finding
# it subsequent times. REQUIRED here is an optionally-present argument that
# should be set to either the string REQUIRED, or empty (this is why it's a macro).
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

# First find: figure out if we need to download
find_arduino("")

if(${ARDUINO_EXECUTABLE} STREQUAL "ARDUINO_EXECUTABLE-NOTFOUND")
  message(STATUS "Didn't find arduino, downloading")
  # Actually download the IDE
  FetchContent_Populate(ARDUINO_IDE)
endif()

# Second find: throw an error if something failed
find_arduino(REQUIRED)

# Run some configurations that make the IDE available
execute_process(
  COMMAND ${ARDUINO_EXECUTABLE} --pref "sketchbook.path=${CMAKE_CURRENT_LIST_DIR}/../arduino-modules" --save-prefs
)
execute_process(
  COMMAND ${ARDUINO_EXECUTABLE} --pref "boardsmanager.additional.urls=https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json" --save-prefs
)
execute_process(
  COMMAND ${ARDUINO_EXECUTABLE} --install-boards "Opentrons:samd:${OPENTRONS_SAMD_BOARDS_VERSION}"
)
execute_process(
  COMMAND ${ARDUINO_EXECUTABLE} --install-boards "arduino:samd:${ARDUINO_SAMD_VERSION}"
)
execute_process(
  COMMAND ${ARDUINO_EXECUTABLE} --install-boards "Opentrons:avr"
)


# Cross-compile setup. Typically you'd set CMAKE_CXX_COMPILER here, but that would cause cmake to try and
# run something with try_compile, and that would fail since the arduino_ide isn't actually a CXX compiler.
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION 0)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
