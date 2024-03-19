# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(${TARGET_MODULE_NAME}
    putchar.c
    task_builder.cpp
    test_host_comms_task.cpp
    test_lid_heater_task.cpp
    test_main.cpp
    test_system_policy.cpp
    test_system_task.cpp
    test_system_pulse.cpp
    test_thermal_plate_task.cpp
    test_plate_control.cpp
    test_peltier_filter.cpp
    test_tmc2130.cpp
    test_board_revision_hardware.cpp
    test_board_revision.cpp
    test_colors.cpp
    test_motor_task.cpp
    test_motor_utils.cpp
    test_eeprom.cpp
    test_errors.cpp
    # GCode parse tests
    test_m14.cpp
    test_m18.cpp
    test_m103d.cpp
    test_m104.cpp
    test_m105.cpp
    test_m105d.cpp
    test_m115.cpp
    test_m141d.cpp
    test_m104d.cpp
    test_m106.cpp
    test_m107.cpp
    test_m108.cpp
    test_m116.cpp
    test_m117.cpp
    test_m119.cpp
    test_m126.cpp
    test_m127.cpp
    test_m128.cpp
    test_m140.cpp
    test_m140d.cpp
    test_m141.cpp
    test_m301.cpp
    test_g28d.cpp
    test_m240d.cpp
    test_m241d.cpp
    test_m242d.cpp
    test_m243d.cpp
    test_m900d.cpp
    test_m901d.cpp
    test_m902d.cpp
    test_m903d.cpp
    test_m904d.cpp
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