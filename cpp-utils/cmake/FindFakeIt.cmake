#[=======================================================================[.rst:
FindFakeIt.cmake
----------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It provides FakeIt (https://github.com/eranpeer/FakeIt/), the C++ mocking
framework, in the variant intended for use with Catch2.

It will download (if necessary) the release-bundled header file to stm32-tools/fakeit
and provide it as an interface library that can be added to a build.

To use FakeIt, find the package and then link FakeIt into your test:

find_package(FakeIt REQUIRED)
add_executable(mytests)
target_link_libraries(mytests FakeIt)

#]=======================================================================]

include(FetchContent)
FetchContent_Declare(
  FakeIt
  GIT_REPOSITORY https://github.com/eranpeer/FakeIt/
  GIT_TAG        2.0.9
  PREFIX         ${CMAKE_SOURCE_DIR}/stm32-tools/fakeit
  SOURCE_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/fakeit/src
  BINARY_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/fakeit/bin
  STAMP_DIR      ${CMAKE_SOURCE_DIR}/stm32-tools/fakeit/stamps)

FetchContent_MakeAvailable(FakeIt)

set(FakeIt_FOUND TRUE)
add_library(FakeIt INTERFACE IMPORTED)
set_target_properties(FakeIt PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/stm32-tools/fakeit/src/single_header/catch")
