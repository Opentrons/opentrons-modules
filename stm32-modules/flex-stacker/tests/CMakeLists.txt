# this CMakeLists.txt file is only used when host-compiling to build tests
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(${TARGET_MODULE_NAME}
        test_main.cpp
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