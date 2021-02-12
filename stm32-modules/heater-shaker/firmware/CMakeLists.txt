# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32F303BSP)

add_executable(heater-shaker
  main.cpp
  system_stm32f3xx.c
  startup_stm32f303xe.s)
target_link_options(heater-shaker
  PRIVATE
  "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/STM32F303RETx_FLASH.ld"
  "LINKER:--error-unresolved-symbols")

target_link_libraries(heater-shaker STM32F303BSP)


target_include_directories(STM32F303BSP
  PUBLIC .)
target_compile_definitions(STM32F303BSP
  PUBLIC STM32F303xE)


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
