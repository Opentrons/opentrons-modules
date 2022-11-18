# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(${TARGET_MODULE_NAME}
    test_main.cpp
    test_heartbeat.cpp
    test_host_comms_task.cpp
    test_eeprom.cpp
    test_errors.cpp
    test_system_task.cpp
    test_ui_task.cpp
    test_thermistor_task.cpp
    test_thermal_task.cpp
    # gcode tests
    test_m18.cpp
    test_m104.cpp
    test_m103d.cpp
    test_m104d.cpp
    test_m105d.cpp
    test_m106.cpp
    test_m107.cpp 
    test_m115.cpp
    test_m116.cpp
    test_m117.cpp
    test_m301.cpp
    test_m996.cpp
    test_dfu_gcode.cpp
)

target_include_directories(${TARGET_MODULE_NAME} 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME})

set_target_properties(${TARGET_MODULE_NAME}
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE)

target_compile_options(${TARGET_MODULE_NAME}
    PUBLIC
    -Wall
    -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
    $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
    $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
    $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

target_link_libraries(${TARGET_MODULE_NAME} 
    ${TARGET_MODULE_NAME}-core 
    common-core
    Catch2::Catch2)

catch_discover_tests(${TARGET_MODULE_NAME} )
add_build_and_test_target(${TARGET_MODULE_NAME} )

add_coverage(${TARGET_MODULE_NAME})