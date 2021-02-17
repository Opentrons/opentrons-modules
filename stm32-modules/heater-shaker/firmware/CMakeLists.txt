# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32F303BSP)
find_package(CrossGCC)
find_package(OpenOCD)

add_executable(heater-shaker
  main.cpp
  system_stm32f3xx.c
  startup_stm32f303xe.s)
target_link_options(heater-shaker
  PRIVATE
  "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/STM32F303RETx_FLASH.ld"
  "LINKER:--print-memory-usage"
  "LINKER:--error-unresolved-symbols")

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/main.cpp
  PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/STM32F303RETx_FLASH.ld")


target_link_libraries(heater-shaker STM32F303BSP)

target_include_directories(STM32F303BSP
  PUBLIC .)
target_compile_definitions(STM32F303BSP
  PUBLIC STM32F303xE)

# Fills in the template with values specified by the find_package(OpenOCD) call above
configure_file(./gdbinit.template ./gdbinit)


find_program(ARM_GDB
  arm-none-eabi-gdb
  PATHS "${CrossGCC_BINDIR}"
  NO_DEFAULT_PATH
  REQUIRED)
# Configure gdb (full path to cross-gdb set in the toolchain) to use the gdbinit in
# this dir
set_target_properties(heater-shaker
  PROPERTIES
  CROSSCOMPILING_EMULATOR
     "${ARM_GDB};--command=${CMAKE_CURRENT_BINARY_DIR}/gdbinit")



# Runs cross-gdb (since CMAKE_CROSSCOMPILING_EMULATOR is set in an
# arguable misuse of the concept) to the appropriate cross-gdb with
# remote-target. You should make sure st-util is running; that's not
# done here because it won't be multi-os compatible, and also it
# should be running the entire time and that's tough to accomplish
# in a custom command
add_custom_target(heater-shaker-debug
  COMMAND heater-shaker
  USES_TERMINAL
  )
