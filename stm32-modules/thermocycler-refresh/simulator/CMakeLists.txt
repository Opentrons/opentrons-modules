find_package(Boost 1.71.0)
add_executable(heater-shaker-simulator
  comm_thread.cpp
  motor_thread.cpp
  heater_thread.cpp
  system_thread.cpp
  main.cpp)

target_link_libraries(heater-shaker-simulator PRIVATE heater-shaker-core Boost::boost pthread)
target_include_directories(heater-shaker-simulator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")


set_target_properties(heater-shaker-simulator
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
