#[=======================================================================[.rst:
FindCatch2.cmake
----------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download (if necessary) the Catch2 (https://github.com/catchorg/Catch2/tree/v2.x)
release-bundled header file to stm32-tools/catch2 and provide it as a target.

#]=======================================================================]

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.8
  PREFIX         ${CMAKE_SOURCE_DIR}/stm32-tools/catch2
  SOURCE_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/catch2/src
  BINARY_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/catch2/bin
  STAMP_DIR      ${CMAKE_SOURCE_DIR}/stm32-tools/catch2/stamps)

FetchContent_MakeAvailable(Catch2)

set(Catch2_FOUND TRUE)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/stm32-tools/catch2/src/contrib)
