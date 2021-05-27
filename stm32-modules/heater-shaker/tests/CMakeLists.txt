# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(heater-shaker
  task_builder.cpp
  test_motor_policy.cpp
  test_heater_policy.cpp
  test_system_policy.cpp
  test_gcode_parse.cpp
  test_basic_gcodes.cpp
  test_m104.cpp
  test_m104d.cpp
  test_m105.cpp
  test_m105d.cpp
  test_m123.cpp
  test_m124.cpp
  test_m3.cpp
  test_m301.cpp
  test_version.cpp
  test_pid.cpp
  test_double_buffer.cpp
  test_host_comms_task.cpp
  test_heater_task.cpp
  test_motor_task.cpp
  test_system_task.cpp
  test_ack_cache.cpp
  test_errors.cpp
  test_message_passing.cpp
  test_thermistor_conversions.cpp
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
target_link_libraries(heater-shaker heater-shaker-core Catch2::Catch2)

catch_discover_tests(heater-shaker)
add_build_and_test_target(heater-shaker)
