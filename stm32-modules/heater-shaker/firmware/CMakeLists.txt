# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32F303BSP)
add_STM32F303_driver(heater-shaker)
add_STM32F303_freertos(heater-shaker)
add_STM32F303_usb(heater-shaker)

set(SYSTEM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/system")
set(HEATER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/heater_task")
set(MOTOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/motor_task")
set(COMMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host_comms_task")
set(COMMON_MCU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/STM32F303")
set(COMMON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/src")
set(GDBINIT_PATH "${CMAKE_CURRENT_BINARY_DIR}/../../common/STM32F303/gdbinit")

set(MCSDK_VERSION 5.4.4)
set(MCSDK_LOCATION "${CMAKE_CURRENT_LIST_DIR}/motor_task/MCSDK_v${MCSDK_VERSION}/MotorControl")

add_library(_mc_static STATIC IMPORTED)
set_target_properties(_mc_static
  PROPERTIES IMPORTED_LOCATION
  "${MCSDK_LOCATION}/lib/libmc-gcc_M4.lib")

file(GLOB _mcsdk_any_sources "${MCSDK_LOCATION}/MCSDK/MCLib/Any/Src/*.c")
file(GLOB _mcsdk_f3xx_sources "${MCSDK_LOCATION}/MCSDK/MCLib/F3xx/Src/*.c")

add_library(mc STATIC
  ${_mcsdk_any_sources}
  ${_mcsdk_f3xx_sources}
  )
set_target_properties(mc
  PROPERTIES C_STANDARD 11
  C_STANDARD_REQUIRED TRUE)
target_compile_definitions(mc PUBLIC CCMRAM)

target_include_directories(mc
  PUBLIC "${MCSDK_LOCATION}/MCSDK/MCLib/Any/Inc"
         "${MCSDK_LOCATION}/MCSDK/MCLib/F3xx/Inc"
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/motor_task")
target_link_libraries(mc PUBLIC _mc_static STM32F303BSP_Drivers_heater-shaker)
target_link_libraries(_mc_static INTERFACE mc)


# Add source files that should be checked by clang-tidy here
set(HS_FW_LINTABLE_SRCS
  ${SYSTEM_DIR}/main.cpp
  ${SYSTEM_DIR}/freertos_system_task.cpp
  ${SYSTEM_DIR}/system_policy.cpp
  ${HEATER_DIR}/freertos_heater_task.cpp
  ${HEATER_DIR}/heater_policy.cpp
  ${MOTOR_DIR}/freertos_motor_task.cpp
  ${MOTOR_DIR}/motor_policy.cpp
  ${COMMS_DIR}/freertos_comms_task.cpp
  ${SYSTEM_DIR}/freertos_idle_timer_task.cpp)

# Add source files that should NOT be checked by clang-tidy here
set(HS_FW_NONLINTABLE_SRCS
  ${SYSTEM_DIR}/system_stm32f3xx.c
  ${SYSTEM_DIR}/stm32f3xx_it.c
  ${COMMS_DIR}/usbd_conf.c
  ${COMMS_DIR}/usbd_desc.c
  ${COMMS_DIR}/uart_hardware.c
  ${MOTOR_DIR}/motor_hardware.c
  ${MOTOR_DIR}/mc_config.c
  ${MOTOR_DIR}/mc_interface.c
  ${MOTOR_DIR}/mc_math.c
  ${MOTOR_DIR}/mc_parameters.c
  ${MOTOR_DIR}/mc_tasks.c
  ${MOTOR_DIR}/stm32f30x_mc_it.c
  ${MOTOR_DIR}/regular_conversion_manager.c
  ${SYSTEM_DIR}/system_hardware.c
  ${HEATER_DIR}/heater_hardware.c
  ${SYSTEM_DIR}/system_serial_number.c
  ${SYSTEM_DIR}/stm32f3xx_hal_msp.c
  )

add_executable(heater-shaker
  ${HS_FW_LINTABLE_SRCS}
  ${HS_FW_NONLINTABLE_SRCS})

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(${SYSTEM_DIR}/main.cpp
  PROPERTIES
  OBJECT_DEPENDS "${COMMON_MCU_DIR}/STM32F303RETx_FLASH.ld")

set_target_properties(heater-shaker
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE
             C_STANDARD 11
             C_STANDARD_REQUIRED TRUE)

target_include_directories(heater-shaker
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/heater-shaker
  PRIVATE ${MOTOR_DIR})

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
  heater-shaker PUBLIC
  STM32F303BSP_Drivers_heater-shaker
  STM32F303BSP_USB_heater-shaker
  STM32F303BSP_FreeRTOS_heater-shaker
  mc 
  common-STM32F303
  heater-shaker-core
  )


target_include_directories(STM32F303BSP_Drivers_heater-shaker
  PUBLIC ${SYSTEM_DIR})
target_include_directories(STM32F303BSP_USB_heater-shaker
  PUBLIC ${COMMS_DIR})
target_include_directories(STM32F303BSP_FreeRTOS_heater-shaker
  PUBLIC ${SYSTEM_DIR})

target_compile_definitions(STM32F303BSP_Drivers_heater-shaker
  PUBLIC STM32F303xE)
target_compile_definitions(STM32F303BSP_USB_heater-shaker
  PUBLIC STM32F303xE)
target_compile_definitions(STM32F303BSP_FreeRTOS_heater-shaker
  PUBLIC STM32F303xE)

set_target_properties(
  STM32F303BSP_FreeRTOS_heater-shaker
  PROPERTIES FREERTOS_HEAP_IMPLEMENTATION "heap_5")

find_program(ARM_GDB
  arm-none-eabi-gdb-py
  PATHS "${CrossGCC_BINDIR}"
  NO_DEFAULT_PATH
  REQUIRED)
message(STATUS "Found svd exe at ${GDBSVDTools_gdbsvd_EXECUTABLE}")
# Configure gdb (full path to cross-gdb set in the toolchain) to use the gdbinit in
# this dir
set_target_properties(heater-shaker
  PROPERTIES
  CROSSCOMPILING_EMULATOR
  "${ARM_GDB};--command=${GDBINIT_PATH}")

find_program(CROSS_OBJCOPY "${CrossGCC_TRIPLE}-objcopy"
  PATHS "${CrossGCC_BINDIR}"
  NO_DEFAULT_PATH
  REQUIRED)

# Calculate and package the integrity info for this module
add_custom_target(heater-shaker-integrity ALL 
    COMMAND ${CROSS_OBJCOPY} heater-shaker -Obinary 
        heater-shaker-scratch.bin --gap-fill=0xFF
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py 
        ${CMAKE_CURRENT_BINARY_DIR}/heater-shaker-scratch.bin
        "heater-shaker"
        "0x08008400"
        ${CMAKE_CURRENT_BINARY_DIR}/heater-shaker-integrity-info
    COMMAND ${CROSS_OBJCOPY} heater-shaker
        --update-section
        .integrity_info=${CMAKE_CURRENT_BINARY_DIR}/heater-shaker-integrity-info
    DEPENDS heater-shaker
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py
)

# The .hex target depends on heater-shaker-integrity in order to generate
# the integrity information expected by the startup app. Therefore, all other
# flashable targets should derive from the initial hex.
add_custom_command(OUTPUT heater-shaker.hex
  COMMAND ${CROSS_OBJCOPY} ARGS heater-shaker "-Oihex" heater-shaker.hex
  DEPENDS heater-shaker-integrity
  DEPENDS ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py
  VERBATIM)
add_custom_target(heater-shaker-hex ALL
  DEPENDS heater-shaker.hex)

add_custom_command(OUTPUT heater-shaker.bin
  COMMAND ${CROSS_OBJCOPY} ARGS 
    heater-shaker.hex 
    "-Iihex" "-Obinary" "--gap-fill=0xFF"
    heater-shaker.bin
  DEPENDS heater-shaker-hex
  VERBATIM)
add_custom_target(heater-shaker-bin ALL
  DEPENDS heater-shaker.bin)

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
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-Werror=implicit-fallthrough")
add_custom_target(heater-shaker-lint
  COMMENT "Linting"
  ALL
  COMMAND ${Clang_CLANGTIDY_EXECUTABLE} ${CLANG_EXTRA_ARGS} -p ${CMAKE_BINARY_DIR} ${HS_FW_LINTABLE_SRCS} ${CORE_LINTABLE_SOURCES} --config=
  DEPENDS heater-shaker-core # Required to include the thermistor_lookups file, and possibly generate it if not present
)

# Runs cross-gdb (since CMAKE_CROSSCOMPILING_EMULATOR is set in an
# arguable misuse of the concept) to the appropriate cross-gdb with
# remote-target. You should make sure st-util is running; that's not
# done here because it won't be multi-os compatible, and also it
# should be running the entire time and that's tough to accomplish
# in a custom command
add_custom_target(heater-shaker-debug
  COMMENT "Starting gdb and openocd"
  COMMAND heater-shaker
  DEPENDS heater-shaker-integrity
  USES_TERMINAL
  )

# Create a Startup App target for this module
stm32f303_startup(heater-shaker-startup heater-shaker)

# Targets to create full image hex and binary files containing both bootloader and application
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex"
    DEPENDS $<TARGET_FILE_DIR:heater-shaker>/heater-shaker.hex
    DEPENDS heater-shaker-startup-hex
    DEPENDS $<TARGET_FILE_DIR:heater-shaker-startup>/heater-shaker-startup.hex
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/hex_combine.py "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex" $<TARGET_FILE_DIR:heater-shaker-startup>/heater-shaker-startup.hex heater-shaker.hex
    VERBATIM
    COMMENT "Generating full image")
add_custom_target(heater-shaker-image-hex ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex"
    DEPENDS heater-shaker)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.bin"
  COMMAND ${CROSS_OBJCOPY} ARGS 
    "-Iihex" "-Obinary"  "--gap-fill=0xFF"
    "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex" 
    "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.bin"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex"
  VERBATIM)
add_custom_target(heater-shaker-image-bin ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.bin")

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.zip"
  DEPENDS heater-shaker-image-hex
  DEPENDS heater-shaker-image-bin
  COMMAND ${CMAKE_COMMAND} -E tar c
          "${CMAKE_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.zip"
          --format=zip
          --
          "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex"
  VERBATIM
  )

# Runs openocd to flash the board (without using a debugger)
add_custom_target(heater-shaker-flash
    COMMAND "${OpenOCD_EXECUTABLE}" 
        "-f" "${OpenOCD_SCRIPTROOT}/board/st_nucleo_f3.cfg" 
        "-c" "program heater-shaker.hex reset;exit"
    VERBATIM
    COMMENT "Flashing board"
    DEPENDS heater-shaker-hex)
# Runs openocd to flash the board with the full image (startup included)
add_custom_target(heater-shaker-image-flash
    COMMAND "${OpenOCD_EXECUTABLE}" 
        "-f" "${OpenOCD_SCRIPTROOT}/board/st_nucleo_f3.cfg" 
        "-c" "program ${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex reset;exit"
    VERBATIM
    COMMENT "Flashing board"
    DEPENDS heater-shaker-image-hex)

add_custom_target(heater-shaker-zip
  DEPENDS "${CMAKE_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.zip")

install(FILES
  "${CMAKE_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.bin"
  "${CMAKE_CURRENT_BINARY_DIR}/heater-shaker@${heater-shaker_VERSION}.hex"
  DESTINATION "heater-shaker"
)
