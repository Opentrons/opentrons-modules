# Sets up a linkable common library for STM32G491 targets

find_package(GDBSVDTools)

set(G4_BOARD_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/stm32g4discovery.cfg")

# Fills in the template with values specified by the find_package(OpenOCD) call above
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdbinit.template ./gdbinit)
set(GDBINIT_PATH "${CMAKE_CURRENT_BINARY_DIR}/gdbinit")

set(STM_HAL_PREFIX "stm32g4xx")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/common/firmware/hal_util.h.in 
    firmware/hal_util.h)

add_library(${TARGET_MODULE_NAME}-STM32G491 STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32g491vetx.s)

target_include_directories(
    ${TARGET_MODULE_NAME}-STM32G491 PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${TARGET_MODULE_NAME}-STM32G491
    PUBLIC ${TARGET_MODULE_NAME}-core)

target_link_options(${TARGET_MODULE_NAME}-STM32G491
    INTERFACE
    "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/STM32G491VETx_FLASH.ld"
    "LINKER:--print-memory-usage"
    "LINKER:--error-unresolved-symbols"
    "LINKER:--gc-sections"
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float")

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(./startup_stm32g491vetx.s
  PROPERTIES
  OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32G491VETx_FLASH.ld)

# ----- STARTUP APP SECTION ----

# Set up BSP for the startup app
find_package(STM32G4xx)
add_STM32G4_driver(startup)

target_compile_definitions(STM32G4xx_Drivers_startup
    PUBLIC STM32G491xx)
# Include this directory to get the hal_conf file
target_include_directories(STM32G4xx_Drivers_startup
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(STM32G491-startup)
target_module_startup(STM32G491-startup)

target_sources(STM32G491-startup PUBLIC 
    startup_stm32g491vetx.s
    startup_system_stm32g4xx.c)
# Link to the F303
target_link_libraries(STM32G491-startup PUBLIC
    STM32G4xx_Drivers_startup)

# Startup Debug target
find_program(ARM_GDB
    arm-none-eabi-gdb-py
    PATHS "${CrossGCC_BINDIR}"
    NO_DEFAULT_PATH
    REQUIRED)
message(STATUS "Found svd exe at ${GDBSVDTools_gdbsvd_EXECUTABLE}")
# Configure gdb (full path to cross-gdb set in the toolchain) to use the gdbinit in
# this dir
set_target_properties(STM32G491-startup
    PROPERTIES
    CROSSCOMPILING_EMULATOR
    "${ARM_GDB};--command=${GDBINIT_PATH}")
# Runs cross-gdb (since CMAKE_CROSSCOMPILING_EMULATOR is set in an
# arguable misuse of the concept) to the appropriate cross-gdb with
# remote-target. You should make sure st-util is running; that's not
# done here because it won't be multi-os compatible, and also it
# should be running the entire time and that's tough to accomplish
# in a custom command
add_custom_target(STM32G491-startup-debug
    COMMENT "Starting gdb and openocd"
    COMMAND STM32G491-startup
    USES_TERMINAL
    )
