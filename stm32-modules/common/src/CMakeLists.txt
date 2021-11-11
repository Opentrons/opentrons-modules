# This CMakeLists.txt handles compiling all the parts of the module
# that are portable between host and cross compilation as a static
# library. It is included in both host and cross configurations.
# TARGET_MODULE_NAME is set by parent dir to specify module

find_package(Python)
find_package(Git QUIET)

add_custom_command(
  COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/generate_thermistor_table.py
    ${CMAKE_CURRENT_SOURCE_DIR}/ntcg104ed104dtdsx.csv
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_thermistor_table.py
          ${CMAKE_CURRENT_SOURCE_DIR}/ntcg104ed104dtdsx.csv
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
         ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

# Configure lintable/nonlintable sources here
set(CORE_LINTABLE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/pid.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/thermistor_conversion.cpp
  )
set(CORE_NONLINTABLE_SOURCES 
  ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  )

add_library(${TARGET_MODULE_NAME}-core STATIC
  ${CORE_LINTABLE_SOURCES}
  ${CORE_NONLINTABLE_SOURCES}
)

set_target_properties(${TARGET_MODULE_NAME}-core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)

target_include_directories(${TARGET_MODULE_NAME}-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}  # need to add this to get the generated conversion header
  )

target_compile_options(${TARGET_MODULE_NAME}-core
  PRIVATE
  -Wall
  -Werror
  -Weffc++
  -Wreorder
  -Wsign-promo
  -Wextra-semi
  -Wctor-dtor-privacy
  -fno-rtti
  )
