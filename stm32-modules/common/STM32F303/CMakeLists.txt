# Sets up a linkable common library for STM32F303 targets

find_package(GDBSVDTools)

# Fills in the template with values specified by the find_package(OpenOCD) call above
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdbinit.template ./gdbinit)

set(STM_HAL_PREFIX "stm32f3xx")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common/firmware/hal_util.h.in 
    firmware/hal_util.h)


add_library(${TARGET_MODULE_NAME}-STM32F303 STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f303xe.s)

target_include_directories(
    ${TARGET_MODULE_NAME}-STM32F303 PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR})
    
target_link_libraries(${TARGET_MODULE_NAME}-STM32F303
    PUBLIC ${TARGET_MODULE_NAME}-core)

target_link_options(${TARGET_MODULE_NAME}-STM32F303
    INTERFACE
    "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/STM32F303RETx_FLASH.ld"
    "LINKER:--print-memory-usage"
    "LINKER:--error-unresolved-symbols"
    "LINKER:--gc-sections"
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float")

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(./startup_stm32f303xe.s
  PROPERTIES
  OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32F303RETx_FLASH.ld)

# ----- STARTUP APP SECTION ----

# Set up BSP for the startup app
find_package(STM32F303BSP)
add_STM32F303_driver(startup)

target_compile_definitions(STM32F303BSP_Drivers_startup
    PUBLIC STM32F303xE)
# Include this directory to get the hal_conf file
target_include_directories(STM32F303BSP_Drivers_startup
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(STM32F303-startup)
target_module_startup(STM32F303-startup)

target_sources(STM32F303-startup PUBLIC 
    startup_stm32f303xe.s)
# Link to the F303
target_link_libraries(STM32F303-startup PUBLIC
    STM32F303BSP_Drivers_startup)
