# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32F303BSP
  COMPONENTS Drivers FreeRTOS USB)
find_package(CrossGCC)
find_package(OpenOCD)

# Add source files that should be checked by clang-tidy here
set(HS_FW_LINTABLE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos_heater_task.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos_motor_task.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos_ui_task.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos_comms_task.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos_idle_timer_task.cpp)

# Add source files that should NOT be checked by clang-tidy here
set(HS_FW_NONLINTABLE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/system_stm32f3xx.c
  ${CMAKE_CURRENT_SOURCE_DIR}/stm32f3xx_it.c
  ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f303xe.s
  ${CMAKE_CURRENT_SOURCE_DIR}/usbd_conf.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usbd_desc.c)

add_executable(heater-shaker
  ${HS_FW_LINTABLE_SRCS}
  ${HS_FW_NONLINTABLE_SRCS})

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

set_target_properties(heater-shaker
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE
             C_STANDARD 11
             C_STANDARD_REQUIRED TRUE)

target_include_directories(heater-shaker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_compile_options(heater-shaker
  PUBLIC
  -Wall
  -Werror
  $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
  $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
  $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
  $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)


target_link_libraries(
  heater-shaker
  STM32F303BSP_Drivers STM32F303BSP_USB STM32F303BSP_FreeRTOS)

target_include_directories(STM32F303BSP_Drivers
  PUBLIC .)
target_include_directories(STM32F303BSP_USB
  PUBLIC .)
target_include_directories(STM32F303BSP_FreeRTOS
  PUBLIC .)

target_compile_definitions(STM32F303BSP_Drivers
  PUBLIC STM32F303xE)
target_compile_definitions(STM32F303BSP_USB
  PUBLIC STM32F303xE)
target_compile_definitions(STM32F303BSP_FreeRTOS
  PUBLIC STM32F303xE)

set_target_properties(
  STM32F303BSP_FreeRTOS
  PROPERTIES FREERTOS_HEAP_IMPLEMENTATION "heap_5")

find_package(GDBSVDTools)

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

find_package(Clang)

# runs clang-tidy https://releases.llvm.org/11.0.1/tools/clang/tools/extra/docs/clang-tidy/index.html
# which is a catch-all static analyzer/linter
# the empty --config= tells clang-tidy to use the .clang-tidy file in the top level
# An odd thing about this target is that it requires the existance of a compiledb, which
# is produced when you build, and may change if you change compilation options, so in a way
# it depends on a build. But we also want to be able to run this when there wasn't a successful
# build, so there's no explicit dependency set.
# This awful transform is required because the implicit includes that gcc knows how to find (e.g. its
# own implementation of the STL) don't get added to the compile db that clang-tidy uses to figure out
# include directories. So we can use the actually fairly cool transform command to turn them all into
# extra-arg invocations and it'll figure it out.
set(CLANG_EXTRA_ARGS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
list(TRANSFORM CLANG_EXTRA_ARGS PREPEND --extra-arg=-I)
# This helps with clang accepting what GCC accepts around the implementations of the message queue
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-frelaxed-template-template-args")
add_custom_target(heater-shaker-lint
  ALL
  COMMAND ${Clang_CLANGTIDY_EXECUTABLE} ${CLANG_EXTRA_ARGS} -p ${CMAKE_BINARY_DIR} ${HS_FW_LINTABLE_SRCS} --config=)

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
