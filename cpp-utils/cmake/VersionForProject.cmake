#[=======================================================================[.rst:
VersionForProject
-----------------

Provides a function to set the version of a subproject from a Git tag
specific to that project.

This module provides the function ``version_for_project``

version_for_project
^^^^^^^^^^^^^^^^^^^

``version_for_project`` uses ``git describe`` to find tags matching the
project name and cmake string functions to pull the version out of it.

Input Variables
+++++++++++++++

``PROJECT_NAME``
A mandatory positional argument specifying the name of the project,
which is used as the ``match`` argument to ``git describe --tags``.

``OUTPUT_VARIABLE``
An optional second positional argument which will set the name of the
output variable.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``${PROJECT_NAME}_VERSION``
  If the second argument is not set, this is where the version goes
``${OUTPUT_VARIABLE}``
  If the second argument is set, a variable named by the second
  argument is used for the output.
#]=======================================================================]

find_package(Git QUIET)
if(GIT_NOTFOUND)
  fatal_error("Couldn't find git, which is needed for version parsing")
endif()


# version_for_project takes a mandatory function naming the project (which
# should be the version tag prefix) and optionally a second argument naming
# an output variable. If the second variable is not specified, the output
# variable is ${PROJECT_NAME}_VERSION
function(VERSION_FOR_PROJECT PROJECT_NAME)

  # Use the optional arg if present, otherwise default
  if(${ARGC} GREATER 1)
    set(OUTPUT_NAME ${ARGV1})
  else()
    set(OUTPUT_NAME "${PROJECT_NAME}_VERSION")
  endif()

  # Actually pull the version from git
  execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "${PROJECT_NAME}*"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE TAGNAME
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Use cmake internal functions rather than shelling out to cut
  # This first call returns @v2.0.0-4-geeb62a0, for instance
  string(REGEX MATCH "@.*$" PREPENDED_VERSION ${TAGNAME})
  # This second call removes the @
  string(SUBSTRING ${PREPENDED_VERSION} 1 -1 VERSION)

  # Use a macro to dynamically determine a variable name
  macro(SET_OUTPUT OUTPUT_VAR_NAME OUTPUT_VAR_VALUE)
    set(${OUTPUT_VAR_NAME} ${OUTPUT_VAR_VALUE} PARENT_SCOPE)
  endmacro()

  set_output(${OUTPUT_NAME} ${VERSION})

endfunction()
