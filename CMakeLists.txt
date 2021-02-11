# CMake 3.19 is the first to include presets
# (https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
cmake_minimum_required(VERSION 3.19)

project("OpentronsModules"
  LANGUAGES CXX C
  DESCRIPTION "Firmware for Opentrons modules")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# These subdirectories contain the code for these modules and the
# CMakeLists for them. The MODULE_TYPE variable is set in the arduino preset.
if (${MODULE_TYPE} STREQUAL "arduino")
  add_subdirectory(arduino-modules)
elseif (${MODULE_TYPE} STREQUAL "stm32")
  add_subdirectory(stm32-modules)
else()
  message(FATAL_ERROR "Module type ${MODULE_TYPE} is not set or unknown, use a preset or -DMODULE_TYPE")
endif()
