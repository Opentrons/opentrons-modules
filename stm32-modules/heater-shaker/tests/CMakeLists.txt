# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(heater-shaker
  putchar.c
  task_builder.cpp
  test_motor_policy.cpp
  test_heater_policy.cpp
  test_system_policy.cpp
  test_basic_gcodes.cpp
  test_m104.cpp
  test_m104d.cpp
  test_m105.cpp
  test_m105d.cpp
  test_m106.cpp
  test_m123.cpp
  test_m124.cpp
  test_m3.cpp
  test_m301.cpp
  test_m115.cpp
  test_m116.cpp
  test_m117.cpp
  test_g28d.cpp
  test_m240d.cpp
  test_m241.cpp
  test_m241d.cpp
  test_m242.cpp
  test_m243.cpp
  test_m994d.cpp
  test_m994.cpp
  test_m995.cpp
  test_m996.cpp
  test_host_comms_task.cpp
  test_heater_task.cpp
  test_motor_task.cpp
  test_system_task.cpp
  test_errors.cpp
  test_flash.cpp
  test_message_passing.cpp
  test_main.cpp)
target_include_directories(heater-shaker 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/heater-shaker)
set_target_properties(heater-shaker
  PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED TRUE)

target_compile_options(heater-shaker
  PUBLIC
  -Wall
  -Werror
  $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
  $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
  $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
  $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
target_link_libraries(heater-shaker 
  heater-shaker-core 
  common-core
  Catch2::Catch2)

catch_discover_tests(heater-shaker)
add_build_and_test_target(heater-shaker)
