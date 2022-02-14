# Sets up a linkable common library for STM32G491 targets

find_package(GDBSVDTools)

set(G4_BOARD_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/stm32g4discovery.cfg")

# Fills in the template with values specified by the find_package(OpenOCD) call above
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdbinit.template ./gdbinit)

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
