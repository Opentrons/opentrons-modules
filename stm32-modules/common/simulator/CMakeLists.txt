add_library(
  ${TARGET_MODULE_NAME}-simulator STATIC
  cli_parser.cpp 
  socket_sim_driver.cpp 
  stdin_sim_driver.cpp
)

target_link_libraries(
  ${TARGET_MODULE_NAME}-simulator 
  PRIVATE ${TARGET_MODULE_NAME}-core 
  Boost::boost pthread
  Boost::program_options
  pthread
)
target_include_directories(
  ${TARGET_MODULE_NAME}-simulator 
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common
  )

set_target_properties(${TARGET_MODULE_NAME}-simulator
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
