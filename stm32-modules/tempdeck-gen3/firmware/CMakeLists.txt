# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32G4xx)
add_STM32G4_driver(${TARGET_MODULE_NAME})
add_STM32G4_freertos(${TARGET_MODULE_NAME})
add_STM32G4_usb(${TARGET_MODULE_NAME})

set(SYSTEM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/system")
set(COMMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host_comms_task")
set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(THERMAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thermal_control")
set(THERMISTOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thermistor")
set(COMMON_MCU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/STM32G491")
set(COMMON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/src")
set(GDBINIT_PATH "${CMAKE_CURRENT_BINARY_DIR}/../../common/STM32G491/gdbinit")

# Add source files that should be checked by clang-tidy here
set(${TARGET_MODULE_NAME}_FW_LINTABLE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp 
  ${SYSTEM_DIR}/freertos_system_task.cpp
  ${SYSTEM_DIR}/freertos_idle_timer_task.cpp
  ${SYSTEM_DIR}/system_policy.cpp 
  ${COMMS_DIR}/freertos_comms_task.cpp
  ${COMMS_DIR}/usb_hardware.c
  ${UI_DIR}/freertos_ui_task.cpp
  ${UI_DIR}/ui_hardware.c
  ${UI_DIR}/ui_policy.cpp
  ${THERMAL_DIR}/freertos_thermal_task.cpp
  ${THERMAL_DIR}/thermal_policy.cpp
  ${THERMISTOR_DIR}/freertos_thermistor_task.cpp
  ${THERMISTOR_DIR}/thermistor_policy.cpp
)

# Add source files that should NOT be checked by clang-tidy here
set(${TARGET_MODULE_NAME}_FW_NONLINTABLE_SRCS
  ${SYSTEM_DIR}/hal_tick.c
  ${SYSTEM_DIR}/system_stm32g4xx.c
  ${SYSTEM_DIR}/system_hardware.c
  ${SYSTEM_DIR}/system_serial_number.c
  ${SYSTEM_DIR}/stm32g4xx_it.c
  ${SYSTEM_DIR}/stm32g4xx_hal_msp.c
  ${THERMAL_DIR}/thermal_hardware.c
  ${THERMISTOR_DIR}/thermistor_hardware.c
  ${COMMS_DIR}/usbd_conf.c
  ${COMMS_DIR}/usbd_desc.c
  )

add_executable(${TARGET_MODULE_NAME}
  ${${TARGET_MODULE_NAME}_FW_LINTABLE_SRCS}
  ${${TARGET_MODULE_NAME}_FW_NONLINTABLE_SRCS})

# Incurs at least a relink when you change the linker file (and a recompile of main
# but hopefully that's quick)
set_source_files_properties(${SYSTEM_DIR}/main.cpp
  PROPERTIES
  OBJECT_DEPENDS "${COMMON_MCU_DIR}/STM32G491VETx_FLASH.ld")

set_target_properties(${TARGET_MODULE_NAME}
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE
             C_STANDARD 11
             C_STANDARD_REQUIRED TRUE)

target_include_directories(${TARGET_MODULE_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp-utils/include/
    )

target_compile_options(${TARGET_MODULE_NAME}
  PUBLIC
  -Wall
  -Werror
  -Wimplicit-fallthrough
  $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
  $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
  $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
  $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)


target_link_libraries(
  ${TARGET_MODULE_NAME} PUBLIC
  STM32G4xx_Drivers_${TARGET_MODULE_NAME} 
  STM32G4xx_FreeRTOS_${TARGET_MODULE_NAME} 
  STM32G4xx_USB_${TARGET_MODULE_NAME} 
  common-STM32G491
  ${TARGET_MODULE_NAME}-core
  )


target_include_directories(STM32G4xx_Drivers_${TARGET_MODULE_NAME}
  PUBLIC ${SYSTEM_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}/firmware)
target_include_directories(STM32G4xx_USB_${TARGET_MODULE_NAME}
  PUBLIC ${COMMS_DIR})
# Add FreertosConfig to the freertos build
target_include_directories(STM32G4xx_FreeRTOS_${TARGET_MODULE_NAME}
  PUBLIC ${SYSTEM_DIR})

target_compile_definitions(STM32G4xx_Drivers_${TARGET_MODULE_NAME}
  PUBLIC STM32G491xx)
target_compile_definitions(STM32G4xx_USB_${TARGET_MODULE_NAME}
  PUBLIC STM32G491xx)
target_compile_definitions(STM32G4xx_FreeRTOS_${TARGET_MODULE_NAME}
  PUBLIC STM32G491xx)

set_target_properties(
  STM32G4xx_FreeRTOS_${TARGET_MODULE_NAME}
  PROPERTIES FREERTOS_HEAP_IMPLEMENTATION "heap_5")


find_program(ARM_GDB
  arm-none-eabi-gdb-py
  PATHS "${CrossGCC_BINDIR}"
  NO_DEFAULT_PATH
  REQUIRED)
message(STATUS "Found svd exe at ${GDBSVDTools_gdbsvd_EXECUTABLE}")
# Configure gdb (full path to cross-gdb set in the toolchain) to use the gdbinit in
# this dir
set_target_properties(${TARGET_MODULE_NAME}
  PROPERTIES
  CROSSCOMPILING_EMULATOR
  "${ARM_GDB};--command=${GDBINIT_PATH}")

find_program(CROSS_OBJCOPY "${CrossGCC_TRIPLE}-objcopy"
  PATHS "${CrossGCC_BINDIR}"
  NO_DEFAULT_PATH
  REQUIRED)

# Calculate and package the integrity info for this module
add_custom_target(${TARGET_MODULE_NAME}-integrity ALL 
  COMMAND ${CROSS_OBJCOPY} ${TARGET_MODULE_NAME} -Obinary 
      ${TARGET_MODULE_NAME}-scratch.bin --gap-fill=0xFF
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py 
      ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_MODULE_NAME}-scratch.bin
      ${TARGET_MODULE_NAME}
      "0x08008400"
      ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_MODULE_NAME}-integrity-info
  COMMAND ${CROSS_OBJCOPY} ${TARGET_MODULE_NAME}
      --update-section
      .integrity_info=${CMAKE_CURRENT_BINARY_DIR}/${TARGET_MODULE_NAME}-integrity-info
  DEPENDS ${TARGET_MODULE_NAME}
  DEPENDS ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py
)

# The .hex target depends on the module's integrity-target in order to generate
# the integrity information expected by the startup app. Therefore, all other
# flashable targets should derive from the initial hex.
add_custom_command(OUTPUT ${TARGET_MODULE_NAME}.hex
  COMMAND ${CROSS_OBJCOPY} ARGS ${TARGET_MODULE_NAME} "-Oihex" ${TARGET_MODULE_NAME}.hex
  DEPENDS ${TARGET_MODULE_NAME}
  DEPENDS ${TARGET_MODULE_NAME}-integrity
  DEPENDS ${CMAKE_SOURCE_DIR}/scripts/calculate_checksum.py
  VERBATIM)
add_custom_target(${TARGET_MODULE_NAME}-hex ALL
  DEPENDS ${TARGET_MODULE_NAME}.hex)

add_custom_command(OUTPUT ${TARGET_MODULE_NAME}.bin
  COMMAND ${CROSS_OBJCOPY} ARGS 
    ${TARGET_MODULE_NAME}.hex 
    "-Iihex" "-Obinary" "--gap-fill=0xFF"
    ${TARGET_MODULE_NAME}.bin
  DEPENDS ${TARGET_MODULE_NAME}.hex
  VERBATIM)
add_custom_target(${TARGET_MODULE_NAME}-bin ALL
  DEPENDS ${TARGET_MODULE_NAME}.bin)
add_dependencies(${TARGET_MODULE_NAME}-bin ${TARGET_MODULE_NAME}-hex)

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
get_target_property(SRC_INCLUDE_DIRS ${TARGET_MODULE_NAME} INCLUDE_DIRECTORIES)
list(APPEND CLANG_EXTRA_ARGS ${SRC_INCLUDE_DIRS})
list(TRANSFORM CLANG_EXTRA_ARGS PREPEND --extra-arg=-I)
# This helps with clang accepting what GCC accepts around the implementations of the message queue
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-frelaxed-template-template-args")
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-Werror=implicit-fallthrough")
add_custom_target(${TARGET_MODULE_NAME}-lint
  ALL
  COMMENT "Linting"
  COMMAND ${Clang_CLANGTIDY_EXECUTABLE} ${CLANG_EXTRA_ARGS} -p ${CMAKE_BINARY_DIR} ${${TARGET_MODULE_NAME}_FW_LINTABLE_SRCS} ${CORE_LINTABLE_SOURCES}
  DEPENDS ${TARGET_MODULE_NAME}-core # Required to include the thermistor_lookups file, and possibly generate it if not present
)

# Runs cross-gdb (since CMAKE_CROSSCOMPILING_EMULATOR is set in an
# arguable misuse of the concept) to the appropriate cross-gdb with
# remote-target. You should make sure st-util is running; that's not
# done here because it won't be multi-os compatible, and also it
# should be running the entire time and that's tough to accomplish
# in a custom command
add_custom_target(${TARGET_MODULE_NAME}-debug
  COMMENT "Starting gdb and openocd"
  COMMAND ${TARGET_MODULE_NAME}
  DEPENDS ${TARGET_MODULE_NAME}-integrity
  USES_TERMINAL
  )

set(HEX_IMG_NAME "${TARGET_MODULE_NAME}@${${TARGET_MODULE_NAME}_VERSION}.hex")

set(BIN_IMG_NAME "${TARGET_MODULE_NAME}@${${TARGET_MODULE_NAME}_VERSION}.bin")
  
set(STARTUP_NAME ${TARGET_MODULE_NAME}-startup)

stm32g491_clear_target(${TARGET_MODULE_NAME}-clear)

stm32g491_startup(${STARTUP_NAME} ${TARGET_MODULE_NAME})

# Targets to create full image hex file containing both bootloader and application
add_custom_command(
    OUTPUT ${HEX_IMG_NAME}
    DEPENDS ${TARGET_MODULE_NAME}-hex
    DEPENDS ${STARTUP_NAME}-hex
    DEPENDS ${TARGET_MODULE_NAME}.hex
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/hex_combine.py ${HEX_IMG_NAME} $<TARGET_FILE_DIR:${STARTUP_NAME}>/${STARTUP_NAME}.hex ${TARGET_MODULE_NAME}.hex
    VERBATIM)
add_custom_target(${TARGET_MODULE_NAME}-image-hex ALL
    DEPENDS ${HEX_IMG_NAME})
add_dependencies(${TARGET_MODULE_NAME}-image-hex
    ${TARGET_MODULE_NAME}-hex)

add_custom_command(OUTPUT "${BIN_IMG_NAME}"
    COMMAND ${CROSS_OBJCOPY} ARGS 
        "-Iihex" "-Obinary" "--gap-fill=0xFF"
        "${HEX_IMG_NAME}" "${BIN_IMG_NAME}"
    DEPENDS "${HEX_IMG_NAME}"
    DEPENDS ${TARGET_MODULE_NAME}-image-hex
    VERBATIM)
add_custom_target(${TARGET_MODULE_NAME}-image-bin ALL
    DEPENDS ${BIN_IMG_NAME})
add_dependencies(${TARGET_MODULE_NAME}-image-bin
    ${TARGET_MODULE_NAME}-image-hex)

set(ZIP_NAME "${CMAKE_BINARY_DIR}/${TARGET_MODULE_NAME}@${${TARGET_MODULE_NAME}_VERSION}.zip")

# Runs openocd to flash the board (without using a debugger)
add_custom_target(${TARGET_MODULE_NAME}-flash
    COMMAND "${OpenOCD_EXECUTABLE}" 
        "-f" "${COMMON_MCU_DIR}/stm32g4discovery.cfg" 
        "-c" "program ${TARGET_MODULE_NAME}.hex reset;exit"
    VERBATIM
    COMMENT "Flashing board"
    DEPENDS ${TARGET_MODULE_NAME}.hex)
# Runs openocd to flash the board with the full image (startup included)
add_custom_target(${TARGET_MODULE_NAME}-image-flash
    COMMAND "${OpenOCD_EXECUTABLE}" 
        "-f" "${COMMON_MCU_DIR}/stm32g4discovery.cfg" 
        "-c" "program ${HEX_IMG_NAME} reset;exit"
    VERBATIM
    COMMENT "Flashing board"
    DEPENDS ${HEX_IMG_NAME})

add_custom_command(
    OUTPUT ${ZIP_NAME}
    DEPENDS ${TARGET_MODULE_NAME}-image-hex
    DEPENDS ${TARGET_MODULE_NAME}-image-bin
    COMMAND ${CMAKE_COMMAND} -E tar c
            "${ZIP_NAME}"
            --format=zip
            --
            "${BIN_IMG_NAME}"
            "${HEX_IMG_NAME}"
    VERBATIM )
    
add_custom_target(${TARGET_MODULE_NAME}-zip
    DEPENDS "${ZIP_NAME}" )
    
install(FILES
    "${ZIP_NAME}"
    "${BIN_IMG_NAME}"
    "${HEX_IMG_NAME}"
    DESTINATION "${TARGET_MODULE_NAME}" )
      