find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
include(AddBuildAndTestTarget)

add_executable(tests
    test_main.cpp
    test_bit_utils.cpp 
    test_fixed_point.cpp
    test_message_utils.cpp
    test_pid.cpp
    test_synchronization.cpp
)

target_include_directories(tests
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
)

set_target_properties(tests
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE)

target_compile_options(tests
    PUBLIC
    -Wall
    -Werror
    -Weffc++
    -Wreorder
    -Wsign-promo
    -Wextra-semi
    -Wctor-dtor-privacy
    -fno-rtti
)

target_link_libraries(tests
    core Catch2::Catch2
)

catch_discover_tests(tests)
add_build_and_test_target(tests)
