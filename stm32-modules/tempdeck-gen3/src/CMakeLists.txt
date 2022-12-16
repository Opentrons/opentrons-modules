# This CMakeLists.txt handles compiling all the parts of the module
# that are portable between host and cross compilation as a static
# library. It is included in both host and cross configurations.
# TARGET_MODULE_NAME is set by parent dir to specify module

find_package(Python)
find_package(Git QUIET)

set(SHARED_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp-utils/src)
# Generate the thermistor table for this project
set(COMMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common/src)

add_custom_command(
  COMMAND Python::Interpreter ${COMMON_SRC_DIR}/generate_thermistor_table.py
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
    --nxffile
    ${COMMON_SRC_DIR}/nxft15xv103fa2b030.csv
  DEPENDS ${COMMON_SRC_DIR}/generate_thermistor_table.py
          ${COMMON_SRC_DIR}/nxft15xv103fa2b030.csv
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
         ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

# This is like version for project but works when there aren't any tags yet
execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "${TARGET_MODULE_NAME}*"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE TAGNAME
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if (NOT TAGNAME)
  set(TAGNAME "@(dev)")
endif()

string(REGEX MATCH "@.*$" PREPENDED_VERSION ${TAGNAME})
string(SUBSTRING "${PREPENDED_VERSION}" 1 -1 VERSION)
if (NOT VERSION)
  set(VERSION "(dev)")
endif()

set(${TARGET_MODULE_NAME}_VERSION "${VERSION}" CACHE STRING "${TARGET_MODULE_NAME} fw version" FORCE)

configure_file(./version.cpp.in ./version.cpp)

option(ALLOW_ASYNC_ERRORS "Select whether asynchronous errors are sent by firmware")

if(ALLOW_ASYNC_ERRORS) 
    add_definitions(-DSYSTEM_ALLOW_ASYNC_ERRORS)
endif()

# Configure lintable/nonlintable sources here
set(CORE_LINTABLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/errors.cpp)
set(CORE_LINTABLE_SOURCES
    ${CORE_LINTABLE_SOURCES}
    PARENT_SCOPE)
set(CORE_NONLINTABLE_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
    ${SHARED_SRC_DIR}/pid.cpp)

add_library(${TARGET_MODULE_NAME}-core STATIC
  ${CORE_LINTABLE_SOURCES}
  ${CORE_NONLINTABLE_SOURCES}
)

set_target_properties(${TARGET_MODULE_NAME}-core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE
             C_STANDARD 11
             C_STANDARD_REQUIRED TRUE)

target_link_libraries(${TARGET_MODULE_NAME}-core PUBLIC common-core)

target_include_directories(${TARGET_MODULE_NAME}-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}  # need to add this to get the generated conversion header
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp-utils/include/
  )

target_compile_options(${TARGET_MODULE_NAME}-core
  PRIVATE
  -Wall
  -Werror
  -Wimplicit-fallthrough
  $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
  $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
  $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
  $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  )

add_coverage(${TARGET_MODULE_NAME}-core)