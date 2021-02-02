function(ADD_ARDUINO_COMMAND)
  set(OPTIONS HEX)
  set(ONE_VALUE_ARGS OUTPUT_NAME SKETCH FQBN)
  set(MULTI_VALUE_ARGS PROJECT_FILES)
  cmake_parse_arguments(ACMD "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
  set(DEFINES "")
  message(STATUS "parsed: sketchfile ${ACMD_SKETCH} output ${ACMD_OUTPUT_NAME} fqbn ${ACMD_FQBN}")
  get_source_file_property(ARDUINO_DEFINES ${ACMD_SKETCH} "ARDUINO_DEFINES")
  message(STATUS "ARDUINO_DEFINES ${ARDUINO_DEFINES}")
  foreach(ARDEF ${ARDUINO_DEFINES})
    message(STATUS "This define: ${ARDEF}")
    get_source_file_property(DEFVAL ${ACMD_SKETCH} ${ARDEF})
    string(APPEND DEFINES "-D${ARDEF}=\"${DEFVAL}\"")
    message(STATUS "DEFINES ${DEFINES}")
  endforeach()
  if (${ACMD_HEX})
    set(TARGET_NAME "${ACMD_OUTPUT_NAME}.hex")
  else()
    set(TARGET_NAME "${ACMD_OUTPUT_NAME}.bin")
  endif()

  add_custom_command(
    OUTPUT "${ACMD_OUTPUT_NAME}.bin"
    MAIN_DEPENDENCY ${ACMD_SKETCH}
    DEPENDS ${ACMD_DEFINES}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIRECTORY}
    VERBATIM
    COMMAND ${ARDUINO_EXECUTABLE} --verify --board ${ACMD_FQBN} --verbose-build --pref compiler.cpp.extra_flags=${DEFINES} ${ACMD_SKETCH})
  add_custom_target(${ACMD_OUTPUT_NAME}
    COMMAND ${CMAKE_COMMAND} -E echo Building ${ACMD_OUTPUT_NAME}
    DEPENDS ${TARGET_NAME}
    VERBATIM
    )
  message("Output name: ${ACMD_OUTPUT_NAME} target name: ${TARGET_NAME}")
endfunction()
