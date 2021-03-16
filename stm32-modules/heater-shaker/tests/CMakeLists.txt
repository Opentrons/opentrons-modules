# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(heater-shaker
  task_builder.cpp
  test_main.cpp)
target_include_directories(heater-shaker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set_target_properties(heater-shaker
  PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED TRUE)

target_compile_options(heater-shaker
  PUBLIC
  -Wall
  -Werror
  -Weffc++
  -Wreorder
  -Wsign-promo
  -Wextra-semi
  -Wctor-dtor-privacy
  -fno-rtti)
target_link_libraries(heater-shaker Catch2::Catch2)

catch_discover_tests(heater-shaker)
add_build_and_test_target(heater-shaker)
