
function(ADD_ARDUINO_COMMAND)
  set(ONE_VALUE_ARGS TARGET_NAME SKETCH FQBN WORKING_DIRECTORY OUTPUT_FORMAT)
  set(MULTI_VALUE_ARGS PROJECT_FILES)
  cmake_parse_arguments(ACMD "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

  # The prop ARDUINO_DEFINES lists the names of the compiler definitions
  get_source_file_property(ARDUINO_DEFINES ${ACMD_SKETCH} "ARDUINO_DEFINES")

  # We can now iterate through that list of names and pull their values, and make them
  # look like compiler args as we do so
  set(DEFINES)
  foreach(ARDEF ${ARDUINO_DEFINES})
    get_source_file_property(DEFVAL ${ACMD_SKETCH} ${ARDEF})
    # If you're going to be smashing your own command strings together instead of using
    # cmake's built in utilities, you have to do this kind of gross testing.
    if(${DEFVAL} STREQUAL "TRUE")
      set(DEFVAL 1)
    elseif(${DEFVAL} STREQUAL "FALSE")
      set(DEFVAL 0)
    elseif(${DEFVAL} MATCHES "^[0-9]+$")
      set(DEFVAL ${DEFVAL})
    else()
      set(DEFVAL \"${DEFVAL}\")
    endif()

    list(APPEND DEFINES "-D${ARDEF}=${DEFVAL}")
  endforeach()

  if(ACMD_WORKING_DIRECTORY)
    set(WORKING_DIRECTORY ${ACMD_WORKING_DIRECTORY})
  else()
    set(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  # This annoying set of gymnastics is required for correct quoting, where the --pref is
  # not in the same quote set as the rest, but the compiler.cpp.extra_flags=-DDEFINE_ONE=1... is.
  if(DEFINES)
    set(DEFINE_PAYLOAD "compiler.cpp.extra_flags=")
    string(JOIN " " DEFINE_STRING ${DEFINES})
    string(APPEND DEFINE_PAYLOAD ${DEFINE_STRING})
    set(DEFINE_CMD --pref ${DEFINE_PAYLOAD})
  else()
    set(DEFINE_CMD "")
  endif()

  get_filename_component(SKETCH_NAME ${ACMD_SKETCH} NAME)
  message(STATUS "Defines for ${ACMD_TARGET_NAME}: ${DEFINE_CMD}")
  # The actual compile command; add_custom_command creates files like a filename: target in make
  add_custom_command(
    OUTPUT "${WORKING_DIRECTORY}/${SKETCH_NAME}.${ACMD_OUTPUT_FORMAT}"
    MAIN_DEPENDENCY ${ACMD_SKETCH}
    DEPENDS ${ACMD_PROJECT_FILES}
    WORKING_DIRECTORY ${WORKING_DIRECTORY}
    VERBATIM
    COMMAND ${ARDUINO_EXECUTABLE} --verify --board ${ACMD_FQBN} --verbose-build ${DEFINE_CMD} --pref "build.path=${CMAKE_CURRENT_BINARY_DIR}" ${ACMD_SKETCH})
  # add_custom_target gives a friendly name like a .PHONY alias in make
  add_custom_target(${ACMD_TARGET_NAME}
    COMMAND ${CMAKE_COMMAND} -E echo Building ${TARGET_NAME}
    DEPENDS "${WORKING_DIRECTORY}/${SKETCH_NAME}.${ACMD_OUTPUT_FORMAT}"
    )
endfunction()
