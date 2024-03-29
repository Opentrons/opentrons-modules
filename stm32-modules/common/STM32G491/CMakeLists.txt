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

# Startup Debug target
find_program(ARM_GDB
    arm-none-eabi-gdb-py
    PATHS "${CrossGCC_BINDIR}"
    NO_DEFAULT_PATH
    REQUIRED)
message(STATUS "Found svd exe at ${GDBSVDTools_gdbsvd_EXECUTABLE}")

function(stm32g491_startup TARGET FW_NAME)
    add_executable(${TARGET})

    target_module_startup(${TARGET} ${FW_NAME})

    target_sources(${TARGET} PUBLIC 
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_stm32g491vetx.s
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_system_stm32g4xx.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_hal.c)

    # Link to the G491 drivers
    target_link_libraries(${TARGET} PUBLIC
        STM32G4xx_Drivers_startup)

    # Configure gdb (full path to cross-gdb set in the toolchain) to use the gdbinit in
    # this dir
    set_target_properties(${TARGET}
        PROPERTIES
        CROSSCOMPILING_EMULATOR
        "${ARM_GDB};--command=${GDBINIT_PATH}")
    # Runs cross-gdb (since CMAKE_CROSSCOMPILING_EMULATOR is set in an
    # arguable misuse of the concept) to the appropriate cross-gdb with
    # remote-target. You should make sure st-util is running; that's not
    # done here because it won't be multi-os compatible, and also it
    # should be running the entire time and that's tough to accomplish
    # in a custom command
    add_custom_target(${TARGET}-debug
        COMMENT "Starting gdb and openocd"
        COMMAND ${TARGET}
        USES_TERMINAL
        )
endfunction()

# Adds a target to clear an STM32F303-based module
function(stm32g491_clear_target TARGET)
    # Each step in this target is forced to succed (|| true) because, when
    # the target reboots to load the option bytes, OpenOCD exits with a
    # failure code because it was not expecting the reboot. For that same
    # reason, each of these steps is its own script because they will not
    # be able to run in a single session sequentially.
    add_custom_target(${TARGET}
        COMMAND ${OpenOCD_EXECUTABLE}
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stm32g4discovery.cfg
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/lock_stm32g491.cfg
            || true
        COMMAND ${OpenOCD_EXECUTABLE}
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stm32g4discovery.cfg
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/unlock_stm32g491.cfg
            || true
        COMMAND ${OpenOCD_EXECUTABLE}
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stm32g4discovery.cfg
            -f ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clear_wp_stm32g491.cfg
            || true
        VERBATIM
        COMMENT "Clearing flash"
    )
endfunction()
