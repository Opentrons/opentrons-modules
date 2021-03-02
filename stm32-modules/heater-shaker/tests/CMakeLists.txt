# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddCheckTarget)

add_executable(heater-shaker
  test_main.cpp)

target_link_libraries(heater-shaker Catch2::Catch2)
catch_discover_tests(heater-shaker)

add_build_and_test_target(heater-shaker)
