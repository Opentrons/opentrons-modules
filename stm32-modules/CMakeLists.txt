# All modules that use STM32 should have their subdirs added here

if (${CMAKE_CROSSCOMPILING})
    # GCC is fetched by toolchain file
    find_package(OpenOCD)
    find_package(GDBSVDTools)
else()
    find_package(Boost 1.71.0)
endif()

find_package(Clang)

add_subdirectory(common)
add_subdirectory(heater-shaker)
add_subdirectory(thermocycler-gen2)
add_subdirectory(tempdeck-gen3)
