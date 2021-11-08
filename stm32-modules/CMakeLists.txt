# All modules that use STM32 should have their subdirs added here

if (${CMAKE_CROSSCOMPILING})
    find_package(CrossGCC)
    find_package(OpenOCD)
endif()

add_subdirectory(heater-shaker)
add_subdirectory(thermocycler-refresh)
