#[=======================================================================[.rst:
FindOpenOCD.cmake
-----------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download (if necessary) openocd (openocd.org) to stm32-tools/openocd/systemname
and provide the paths and configuration data found there.

Provides the variables
- ``OpenOCD_FOUND``: bool, true if found
- ``OpenOCD_EXECUTABLE``: the path to the binary
- ``OpenOCD_SCRIPTROOT``: the path to where the board/chip definition scripts
built into openocd live
- ``OpenOCD_VERSION``: The version

#]=======================================================================]

include(FetchContent)

set(OPENOCD_VERSION "0.11.0-1")
set(OPENOCD_DIR "${CMAKE_SOURCE_DIR}/stm32-tools/openocd")

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(OPENOCD_ARCHIVE_EXTENSION "linux-x64.tar.gz")
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(OPENOCD_ARCHIVE_EXTENSION "darwin-x64.tar.gz")
else()
  set(OPENOCD_ARCHIVE_EXTENSION "win32-x64.zip")
endif()

set(OPENOCD_ARCHIVE "xpack-openocd-${OPENOCD_VERSION}-${OPENOCD_ARCHIVE_EXTENSION}")
set(OPENOCD_URL "https://github.com/xpack-dev-tools/openocd-xpack/releases/download")

FetchContent_Declare(
  OPEN_OCD
  PREFIX ${OPENOCD_DIR}
  SOURCE_DIR "${OPENOCD_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${OPENOCD_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "${OPENOCD_URL}/v${OPENOCD_VERSION}/${OPENOCD_ARCHIVE}")

FetchContent_Populate(OPEN_OCD)

find_program(OPENOCD_EXECUTABLE
  openocd
  PATHS "${OPENOCD_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  PATH_SUFFIXES bin
  NO_DEFAULT_PATH
  REQUIRED)
get_filename_component(OPENOCD_BINDIR ${OPENOCD_EXECUTABLE} DIRECTORY)

set(OpenOCD_FOUND TRUE)
set(OpenOCD_EXECUTABLE ${OPENOCD_EXECUTABLE})
set(OpenOCD_SCRIPTROOT "${OPENOCD_BINDIR}/../scripts")
set(OpenOCD_VERSION "${OPENOCD_VERSION}")
