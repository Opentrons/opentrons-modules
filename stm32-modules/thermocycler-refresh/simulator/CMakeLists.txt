find_package(Boost 1.71.0)
add_executable(${TARGET_MODULE_NAME}-simulator
  main.cpp)

target_link_libraries(${TARGET_MODULE_NAME}-simulator PRIVATE heater-shaker-core Boost::boost pthread)
target_include_directories(${TARGET_MODULE_NAME}-simulator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")


set_target_properties(${TARGET_MODULE_NAME}-simulator
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
