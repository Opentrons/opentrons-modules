# All modules that use STM32 should have their subdirs added here

if (${CMAKE_CROSSCOMPILING})
    find_package(CrossGCC)
    find_package(OpenOCD)
else()
    find_package(Boost 1.71.0)
endif()

add_subdirectory(common)
add_subdirectory(heater-shaker)
add_subdirectory(thermocycler-refresh)
