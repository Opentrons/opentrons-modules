#[=======================================================================[.rst:
FindGDBSVDTools.cmake
-----------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download the core python file from https://github.com/1udo6arre/svd-tools,
create a virtualenv, and install into the virtualenv terminaltables and cmsis-svd.

Provides the variables
- ``GDBSVDTools_FOUND``: bool, true if found
- ``GDBSVDTools_EXECUTABLE``: the path to the individual file gdb-svd.py which needs
to be passed in the gdb init script
- ``GDBSVDTools_VERSION``: The version

#]=======================================================================]

include(FetchContent)

set(GDBSVDTools_DIRECTORY "${CMAKE_SOURCE_DIR}/stm32-tools/gdbsvdtools")
set(DOWNLOADS_DIR ${CMAKE_SOURCE_DIR}/stm32-tools/downloads)

FetchContent_Declare(
  GDBSVDTools_repo
  PREFIX ${GDBSVDTools_DIRECTORY}/gdb-svd
  SOURCE_DIR ${GDBSVDTools_DIRECTORY}/gdb-svd/gdb-svd
  GIT_REPOSITORY "https://github.com/1udo6arre/svd-tools"
  GIT_SHALLOW
  )

FetchContent_MakeAvailable(GDBSVDTools_repo)
FetchContent_GetProperties(GDBSVDTools_repo
  SOURCE_DIR gdbsvdtools_source
  POPULATED gdbsvdtools_populated)

FetchContent_Declare(
  CMSIS_SVD
  GIT_REPOSITORY "https://github.com/posborne/cmsis-svd"
  GIT_TAG 207675b6d22fa2fd733b87a9fb068dcf97d85f99
  GIT_SHALLOW
  PREFIX ${GDBSVDTools_DIRECTORY}/cmsis-svd
  SOURCE_DIR ${GDBSVDTools_DIRECTORY}/cmsis-svd/cmsis-svd
  )

FetchContent_MakeAvailable(CMSIS_SVD)
FetchContent_GetProperties(CMSIS_SVD
  SOURCE_DIR cmsissvd_source
  POPULATED cmsissvd_populated)

FetchContent_Declare(
  TERMINALTABLES
  PREFIX ${GDBSVDTools_DIRECTORY}/terminaltables
  SOURCE_DIR ${GDBSVDTools_DIRECTORY}/terminaltables/terminaltables
  URL "https://files.pythonhosted.org/packages/9b/c4/4a21174f32f8a7e1104798c445dacdc1d4df86f2f26722767034e4de4bff/terminaltables-3.1.0.tar.gz"
  )
FetchContent_MakeAvailable(TERMINALTABLES)
FetchContent_GetProperties(TERMINALTABLES
  SOURCE_DIR terminaltables_source
  POPULATED terminaltables_populated)

FetchContent_Declare(
  SIX_repo
  PREFIX ${GDBSVDTools_DIRECTORY}/six
  SOURCE_DIR ${GDBSVDTools_DIRECTORY}/six/six
  GIT_REPOSITORY "https://github.com/benjaminp/six"
  GIT_SHALLOW
  GIT_TAG "1.15.0")
FetchContent_MakeAvailable(SIX_repo)
FetchContent_GetProperties(SIX_repo
  SOURCE_DIR six_source
  POPULATED six_populated)

if(terminaltables_populated AND cmsissvd_populated AND gdbsvdtools_populated AND six_populated)
  set(GDBSVDTools_POPULATED TRUE)
  set(GDBSVDTools_gdbsvd_POPULATED TRUE)
  set(GDBSVDTools_gdbsvd_EXECUTABLE "${gdbsvdtools_source}/gdb-svd.py")
  set(GDBSVDTools_cmsissvd_POPULATED TRUE)
  set(GDBSVDTools_cmsissvd_DIRECTORY "${cmsissvd_source}")
  set(GDBSVDTools_terminaltables_POPULATED TRUE)
  set(GDBSVDTools_terminaltables_DIRECTORY "${terminaltables_source}")
  set(GDBSVDTools_six_POPULATED TRUE)
  set(GDBSVDTools_six_DIRECTORY "${six_source}")
elseif()
  set(GDBSVDTools_POPULATED "GDBSVDTools_NOTFOUND")
  set(GDBSVDTools_EXECUTABLE "GDBSVDTools_NOTFOUND")
  message(WARNING "Couldn't find GDB SVD tools")
endif()
