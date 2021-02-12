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
  # Set a default build type if none was specified
  # From https://blog.kitware.com/cmake-and-the-default-build-type/

  # MinSizeRel is the default release type since the binary size is quite important.
  set(default_build_type "Debug")
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      "Debug" "MinSizeRel" "RelWithDebInfo")
  endif()
  message("Build type is ${CMAKE_BUILD_TYPE}")
  add_subdirectory(stm32-modules)
else()
  message(FATAL_ERROR "Module type ${MODULE_TYPE} is not set or unknown, use a preset or -DMODULE_TYPE")
endif()
