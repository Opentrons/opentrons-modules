# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)

add_executable(heater-shaker-tests
  test_main.cpp)

target_link_libraries(heater-shaker-tests Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(heater-shaker-tests)
