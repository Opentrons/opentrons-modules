# Sets up a flashable hex file for our STM32 targets
# Creates an executable target named `module-startup`

find_package(GDBSVDTools)

find_program(CROSS_OBJCOPY "${CrossGCC_TRIPLE}-objcopy"
    PATHS "${CrossGCC_BINDIR}"
    NO_DEFAULT_PATH
    REQUIRED)

# Function to configure a target to compile the module startup for a specific
# microcontroller. The target passed in should add an include directory with a
# file `startup_hal.h` containing target-specific definitions
function(target_module_startup TARGET)

    target_sources(${TARGET} PUBLIC
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_main.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_checks.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_it.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_jumps.c)
    
    target_include_directories(${TARGET} 
        PUBLIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR})

    target_compile_options(${TARGET} PUBLIC
        -Wall
        -Werror)

    target_link_options(${TARGET} PUBLIC
        "LINKER:-T,${CMAKE_CURRENT_FUNCTION_LIST_DIR}/STM32_MODULE_STARTUP.ld"
        "LINKER:--print-memory-usage"
        "LINKER:--error-unresolved-symbols"
        "LINKER:--gc-sections")

    set_target_properties(${TARGET}
        PROPERTIES  C_STANDARD 11
                    C_STANDARD_REQUIRED TRUE)

    add_custom_command(OUTPUT ${TARGET}.hex
        COMMAND ${CROSS_OBJCOPY} ARGS ${TARGET} "-Oihex" ${TARGET}.hex
        DEPENDS ${TARGET}
        VERBATIM)
    
    add_custom_target(${TARGET}-hex ALL
        DEPENDS ${TARGET}.hex)
    
    add_custom_command(OUTPUT ${TARGET}.bin
        COMMAND ${CROSS_OBJCOPY} ARGS ${TARGET} "-Obinary" ${TARGET}.bin
        DEPENDS ${TARGET}
        VERBATIM)
    add_custom_target(${TARGET}-bin ALL
        DEPENDS ${TARGET}.bin)
    
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/startup_main.c
        PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/STM32_MODULE_STARTUP.ld)
endfunction()

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(./module-startup.S
    PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32_MODULE_STARTUP.ld)
