add_executable(
        heater-shaker-simulator
        cli_parser.cpp
        comm_thread.cpp
        motor_thread.cpp
        heater_thread.cpp
        socket_sim_driver.cpp
        stdin_sim_driver.cpp
        system_thread.cpp
        main.cpp
)

target_link_libraries(
        heater-shaker-simulator
        PRIVATE heater-shaker-core
        Boost::boost
        Boost::program_options
        pthread
)
target_include_directories(
        heater-shaker-simulator 
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/heater-shaker
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common)


set_target_properties(heater-shaker-simulator
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
