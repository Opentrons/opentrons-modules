#[=======================================================================[.rst:
FindCrossGCC.cmake
------------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download (if necessary) the appropriate gcc cross-compiler for arm to
stm32-tools/gcc-arm-embedded/systemname and provide the paths and configuration
data found there

Provides the variables
- ``CrossGCC_FOUND``: bool, true if found
- ``CrossGCC_BINDIR``: The path to the binary dir where ``${GCC_CROSS_TRIPLE}-gdb``,
``${GCC_CROSS_TRIPLE}-gcc``, etc can be found
- ``CrossGCC_TRIPLE``: The target arch triple (i.e., ``arm-none-eabi``)
- ``CrossGCC_VERSION``: The version
#]=======================================================================]

include(FetchContent)
set(GCC_CROSS_DIR "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/gcc-arm-embedded")
set(GCC_CROSS_VERSION "10-2020-q4-major" CACHE STRING "GCC compiler version")
set(GCC_CROSS_TRIPLE "arm-none-eabi")

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(GCC_ARCHIVE_EXTENSION "x86_64-linux.tar.bz2")
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(GCC_ARCHIVE_EXTENSION "mac.tar.bz2")
else()
  set(GCC_ARCHIVE_EXTENSION "win32.zip")
endif()

set(GCC_ARCHIVE "gcc-arm-none-eabi-${GCC_CROSS_VERSION}-${GCC_ARCHIVE_EXTENSION}")

FetchContent_Declare(
  GCC_ARM_EMBEDDED
  PREFIX ${GCC_CROSS_DIR}
  SOURCE_DIR "${GCC_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${GCC_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/${GCC_ARCHIVE}"
)

macro(FIND_CROSS_GCC REQUIRED)
  find_program(
    CROSS_GCC_EXECUTABLE
    arm-none-eabi-gcc
    PATHS "${GCC_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}/bin"
    ${REQUIRED})
endmacro()

# First call: do we have it?
find_cross_gcc("")

if(${CROSS_GCC_EXECUTABLE} STREQUAL "CROSS_GCC_EXECUTABLE-NOTFOUND")
  message(STATUS "Didn't find cross gcc, downloading")
  FetchContent_Populate(GCC_ARM_EMBEDDED)
endif()

find_cross_gcc(REQUIRED)

get_filename_component(CROSS_GCC_BINDIR ${CROSS_GCC_EXECUTABLE} DIRECTORY)

set(CrossGCC_FOUND TRUE)
set(CrossGCC_DIR ${GCC_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME})
set(CrossGCC_BINDIR ${CROSS_GCC_BINDIR})
set(CrossGCC_TRIPLE ${GCC_CROSS_TRIPLE})
set(CrossGCC_VERSION ${GCC_CROSS_VERSION})
