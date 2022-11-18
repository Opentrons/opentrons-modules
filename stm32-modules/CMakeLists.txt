# All modules that use STM32 should have their subdirs added here

if (${CMAKE_CROSSCOMPILING})
    find_package(CrossGCC)
    find_package(OpenOCD)
    find_package(GDBSVDTools)

    option(ENABLE_ASSERTIONS 
        "If this option is enabled, the firmware will include locking assertions")
    
    if(ENABLE_ASSERTIONS)
        # Enable assertions for all stm32 targets
        add_definitions(-DASSERTIONS_ENABLED)
    endif()

    function(add_coverage TARGET)
    endfunction()

    function(coverage_evaluate)
    endfunction()
else()
    find_package(Boost 1.71.0)
    find_package(codecov)

    # We can safely ignore test code and stm32-tools imports
    list(APPEND LCOV_REMOVE_PATTERNS 
        "'${PROJECT_SOURCE_DIR}/stm32-tools/*'"
        "'${PROJECT_SOURCE_DIR}/**/tests/*.cpp'"
        "'${PROJECT_SOURCE_DIR}/*/putchar.c'")
endif()

find_package(Clang)
find_package(MpalandPrintf)

add_subdirectory(common)
add_subdirectory(heater-shaker)
add_subdirectory(thermocycler-gen2)
add_subdirectory(tempdeck-gen3)

coverage_evaluate()