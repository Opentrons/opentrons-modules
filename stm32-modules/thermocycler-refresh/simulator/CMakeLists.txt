add_executable(
  ${TARGET_MODULE_NAME}-simulator
  cli_parser.cpp
  comm_thread.cpp
  lid_heater_thread.cpp
  socket_sim_driver.cpp
  stdin_sim_driver.cpp
  system_thread.cpp 
  thermal_plate_thread.cpp
  sim_board_revision_hardware.cpp
  motor_thread.cpp
  main.cpp
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
  )


set_target_properties(${TARGET_MODULE_NAME}-simulator
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
