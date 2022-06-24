#[=======================================================================[.rst:
Arduino
-------

Provides a function to replace ``add_executable`` (sadly, not ``target_add_executable``,
since it works using add_custom_command) to build an arduino sketch using the arduino IDE's
command-line interface specific to that project with the proper boards and so on.

It is necessary because the Arduino ide only accepts inputs that end in ``.ino``. That means
that CMake's built-in compiler sensing, which actually works pretty well with most compilation
setups now that you can tell it to not try to link anything, cannot possibly work and it gets
very sad. So we kind of opt out of the normal CMake compiler machinery and resort to use of
``add_custom_command``.

This module provides the function ``add_arduino_command``

add_arduino_command
^^^^^^^^^^^^^^^^^^^

``version_for_project`` uses ``add_custom_command`` to add a build system target to compile
an arduino-based module's sketch with proper dependency management, and ``add_custom_target``
to make it available with a nice alias.

Input Variables
+++++++++++++++

All input variables are by keyword (they are parsed using ``cmake_parse_arguments``).

``TARGET_NAME`` Mandatory keyword argument, single-valued. Specifies the name of the target that will
be created; it can be used after the invocation of ``add_arduino_command`` in, for instance,
an ``add_dependencies`` call.

``SKETCH`` Mandatory keyword argument, single-valued. The full path to the sketch (the file whose name
ends in ``.ino``) to compile.

``FQBN`` Mandatory keyword argument, single-valued. The fully qualified board name for the arduino IDE
to use.

``OUTPUT_FORMAT`` Mandatory keyword argument, single-valued. Should be either `hex` or `bin`. Used to
find the ``objcopy``d output of the compile. For instance, if ``hex``, the system will look for
``${SKETCH_NAME}.hex``.

``WORKING_DIRECTORY`` Optional keyword argument, single-valued. If specified, passed directly to
``add_custom_command``. If not specified, the default is ``CMAKE_CURRENT_BINARY_DIR``.

``PROJECT_FILES`` Optional keyword argument, list-valued. If specified, passed as dependencies to
``add_custom_command``.

Result Variables
^^^^^^^^^^^^^^^^

This will define a target called ``${TARGET_NAME}``, and a file suitable for use as a dependency called
``${SKETCH_NAME}.${OUTPUT_FORMAT}``.

Other Factors
^^^^^^^^^^^^^

Extra compiler defines (passed with ``-D``) for the arduino compiler are read from the file properties
of the sketch file (specified in ``SKETCH_FILE``). This is an ugly workaround. To get it to work, you
must

- Make sure the _name_ of the variable is in a list in the property ``ARDUINO_DEFINES``
- Set a property with that name and the value on the file

There is some (fairly rough) type analysis so that TRUE or FALSE-valued defines will end up as 1 or 0,
numerical (``^[0-9]+$``)-valued defines will end up in there literally, and everything else will be
enclosed by quotes.


#]=======================================================================]


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
