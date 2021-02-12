#[=======================================================================[.rst:
ArduinoToolchain
----------------

This module should never be imported and used in a CMakeLists.txt file at either
build or configuration time. It is only intended to be used as a toolchain specified
either in a preset or with the ``-DCMAKE_TOOLCHAIN_FILE`` command-line option.

It will download the appropriate gcc cross-compiler for arm to
stm32-tools/gcc-arm-embedded and set some basic compile and linker flags (the
CPU flags specified with ``-m``).
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

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/../vendor")
set(CMAKE_SYSROOT "${CROSS_GCC_BINDIR}/${GCC_CROSS_TRIPLE}")
set(CMAKE_C_COMPILER "${CROSS_GCC_BINDIR}/${GCC_CROSS_TRIPLE}-gcc")
set(CMAKE_ASM_COMPILER "${CROSS_GCC_BINDIR}/${GCC_CROSS_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${CROSS_GCC_BINDIR}/${GCC_CROSS_TRIPLE}-g++")

set(GCC_CROSS_BASE_FLAGS
  "-mthumb -march=armv7e-m -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_COMPILER_TARGET thumbv7m-unknown-none-eabi)
set(CMAKE_CXX_COMPILER_TARGET thumbv7m-unknown-none-eabi)
set(CMAKE_C_FLAGS ${GCC_CROSS_BASE_FLAGS})
set(CMAKE_CXX_FLAGS ${GCC_CROSS_BASE_FLAGS})
set(CMAKE_ASM_FLAGS ${GCC_CROSS_BASE_FLAGS})
set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs")

set(CMAKE_CROSSCOMPILING_EMULATOR "${CROSS_GCC_BINDIR}/${GCC_CROSS_TRIPLE}-gdb;--eval-command=tar ext :4242")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
