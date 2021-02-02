# CMake 3.19 is the first to include presets
# (https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
cmake_minimum_required(VERSION 3.19)

project("OpentronsModules"
  LANGUAGES CXX
  DESCRIPTION "Firmware for Opentrons modules")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# These subdirectories contain the code for these modules and the
# CMakeLists for them
add_subdirectory(modules/mag-deck)
#add_subdirectory(modules/temp-deck)
#add_subdirectory(modules/thermo-cycler)
