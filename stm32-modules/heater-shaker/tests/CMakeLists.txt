# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddCheckTarget)

add_executable(heater-shaker-tests
  test_main.cpp)

target_link_libraries(heater-shaker-tests Catch2::Catch2)
catch_discover_tests(heater-shaker-tests)

add_check_target(heater-shaker-tests)
