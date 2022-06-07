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
- ``CrossGCC_BINDIR``: The path to the binary dir where ``${GCC11_CROSS_TRIPLE}-gdb``,
``${GCC11_CROSS_TRIPLE}-gcc``, etc can be found
- ``CrossGCC_TRIPLE``: The target arch triple (i.e., ``arm-none-eabi``)
- ``CrossGCC_VERSION``: The version
#]=======================================================================]

include(FetchContent)
set(GCC11_CROSS_DIR "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/xpack-11-arm-embedded")
set(GCC11_CROSS_VERSION "11.2.1-1.1" CACHE STRING "GCC compiler version")
set(GCC11_CROSS_TRIPLE "arm-none-eabi")

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(GCC11_ARCHIVE_EXTENSION "tar.gz")
  set(GCC11_HOST_STR "linux-x64")
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(GCC11_ARCHIVE_EXTENSION "tar.gz")
  set(GCC11_HOST_STR "darwin-x64")
else()
  set(GCC11_ARCHIVE_EXTENSION "zip")
  set(GCC11_HOST_STR "win32-x64")
endif()

set(GCC_ARCHIVE "xpack-arm-none-eabi-gcc-${GCC11_CROSS_VERSION}-${GCC11_HOST_STR}.${GCC11_ARCHIVE_EXTENSION}")
set(GCC_URL_START "https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v${GCC11_CROSS_VERSION}")

FetchContent_Declare(
  GCC11_ARM_EMBEDDED
  PREFIX ${GCC11_CROSS_DIR}
  SOURCE_DIR "${GCC11_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${GCC11_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "${GCC_URL_START}/${GCC_ARCHIVE}"
)

macro(FIND_CROSS_GCC11 REQUIRED)
  find_program(
    CROSS_GCC11_EXECUTABLE
    arm-none-eabi-GCC
    PATHS "${GCC11_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}/bin"
    ${REQUIRED}
    NO_DEFAULT_PATH)
endmacro()

# First call: do we have it?
find_cross_gcc11("")

if(${CROSS_GCC11_EXECUTABLE} STREQUAL "CROSS_GCC11_EXECUTABLE-NOTFOUND")
  message(STATUS "Didn't find cross gcc11, downloading")
  FetchContent_Populate(GCC11_ARM_EMBEDDED)
else()
  message(STATUS "Found cross gcc11: ${CROSS_GCC11_EXECUTABLE}")
endif()

find_cross_gcc11(REQUIRED)

get_filename_component(CROSS_GCC_BINDIR ${CROSS_GCC11_EXECUTABLE} DIRECTORY)

set(CrossGCC_FOUND TRUE)
set(CrossGCC_DIR ${GCC11_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME})
set(CrossGCC_BINDIR ${CROSS_GCC_BINDIR})
set(CrossGCC_TRIPLE ${GCC11_CROSS_TRIPLE})
set(CrossGCC_VERSION ${GCC11_CROSS_VERSION})
