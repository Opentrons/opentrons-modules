# This CMakeLists.txt handles compiling all the parts of the heater-shaker
# module that are portable between host and cross compilation as a static
# library. It is included in both host and cross configurations.

add_library(heater-shaker-core STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/pid.cpp
  )

set_target_properties(heater-shaker-core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)

target_include_directories(heater-shaker-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_compile_options(heater-shaker-core
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
