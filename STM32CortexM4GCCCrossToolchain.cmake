#[=======================================================================[.rst:
STM32CortexM4GCCCrossToolchain.cmake
----------------------------

This module should never be imported and used in a CMakeLists.txt file at either
build or configuration time. It is only intended to be used as a toolchain specified
either in a preset or with the ``-DCMAKE_TOOLCHAIN_FILE`` command-line option.

It will download the appropriate gcc cross-compiler for arm to
stm32-tools/gcc-arm-embedded and set some basic compile and linker flags (the
CPU flags specified with ``-m``).
#]=======================================================================]

find_package(CrossGCC)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/../vendor")
set(CMAKE_SYSROOT "${CrossGCC_DIR}/${CrossGCC_TRIPLE}")
set(CMAKE_C_COMPILER "${CrossGCC_BINDIR}/${CrossGCC_TRIPLE}-gcc")
set(CMAKE_ASM_COMPILER "${CrossGCC_BINDIR}/${CrossGCC_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${CrossGCC_BINDIR}/${CrossGCC_TRIPLE}-g++")

set(GCC_CROSS_BASE_FLAGS
  "-mthumb -mcpu=cortex-m4 -mfloat-abi=hard -specs=nosys.specs -specs=nano.specs -fpic -ffunction-sections -fdata-sections -fno-lto")
set(CMAKE_C_COMPILER_TARGET thumbv7m-unknown-none-eabi)
set(CMAKE_CXX_COMPILER_TARGET thumbv7m-unknown-none-eabi)
set(CMAKE_C_FLAGS ${GCC_CROSS_BASE_FLAGS})
set(CMAKE_CXX_FLAGS ${GCC_CROSS_BASE_FLAGS})
set(CMAKE_ASM_FLAGS ${GCC_CROSS_BASE_FLAGS})

set(CMAKE_CROSSCOMPILING_EMULATOR "${CrossGCC_BINDIR}/${CrossGCC_TRIPLE}-gdb")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
