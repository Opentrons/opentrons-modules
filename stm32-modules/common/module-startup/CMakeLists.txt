# Sets up a flashable hex file for our STM32 targets
# Creates an executable target named `module-startup`

find_package(GDBSVDTools)

add_executable(module-startup ./module-startup.S)

target_compile_options(module-startup
    PUBLIC
    -Wall
    -Werror)

target_link_options(module-startup
    PUBLIC
    "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/STM32_MODULE_STARTUP.ld"
    "LINKER:--print-memory-usage"
    "LINKER:--error-unresolved-symbols"
    "LINKER:--gc-sections")

find_program(CROSS_OBJCOPY "${CrossGCC_TRIPLE}-objcopy"
    PATHS "${CrossGCC_BINDIR}"
    NO_DEFAULT_PATH
    REQUIRED)

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(./module-startup.s
    PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32_MODULE_STARTUP.ld)

add_custom_command(OUTPUT module-startup.hex
    COMMAND ${CROSS_OBJCOPY} ARGS module-startup "-Oihex" module-startup.hex
    DEPENDS module-startup
    VERBATIM)

add_custom_target(module-startup-hex ALL
    DEPENDS module-startup.hex)


add_custom_command(OUTPUT module-startup.bin
    COMMAND ${CROSS_OBJCOPY} ARGS module-startup "-Obinary" module-startup.bin
    DEPENDS module-startup
    VERBATIM)
add_custom_target(module-startup-bin ALL
    DEPENDS module-startup.bin)
