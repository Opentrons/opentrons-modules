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

else()
    find_package(Boost 1.71.0)
endif()

find_package(Clang)
find_package(MpalandPrintf)

add_subdirectory(common)
add_subdirectory(heater-shaker)
add_subdirectory(thermocycler-gen2)
add_subdirectory(tempdeck-gen3)
