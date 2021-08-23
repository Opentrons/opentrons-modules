#[=======================================================================[.rst:
FindClang.cmake
---------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download (if necessary) the Clang/llvm binaries and appropriate tooling
to stm32-tools/clang/systemname and provide the paths and configuration data found
there.

Unlike other find packages in this dir, though, it is able to use system installs of the
various tools; this can help in situations where the tools are preinstalled, or the
system setup isn't terribly friendly to cmake system detection (for instance, distros
are hard to detect in cmake, so this just downloads the ubuntu distribution of clang
if necessary).

It will message if it chooses to download a tool.

It uses package components to provide not only the compiler and appropriate settings
but other useful tools built on the clang frontend, such as clang-format and CodeChecker.

Provides the variables
- ``Clang_FOUND``: bool, true if found
- ``Clang_EXECUTABLE``: the path to the clang frontend binary
- ``Clang_CLANGTIDY_EXECUTABLE``: the path to clang-tidy
- ``Clang_CLANGFORMAT_EXECUTABLE``: the path to clang-format
- ``Clang_CODECHECKER_EXECUTABLE``: the path to CodeChecker

#]=======================================================================]

include(FetchContent)

set(LOCALINSTALL_CLANG_DIR "${CMAKE_SOURCE_DIR}/stm32-tools/clang")
message(STATUS "local install clang dir: ${LOCALINSTALL_CLANG_DIR}")

set(DL_CLANG_VERSION "12.0.0")

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
  set(CLANG_ARCHIVE "x86_64-linux-gnu-ubuntu-20.04.tar.xz")
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(CLANG_ARCHIVE "x86_64-apple-darwin.tar.xz")
else()
  set(CLANG_ARCHIVE "LLVM-${DL_CLANG_VERSION}-win64.exe")
endif()
FetchContent_Declare(CLANG_LOCALINSTALL
  PREFIX "${LOCALINSTALL_CLANG_DIR}"
  SOURCE_DIR "${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${DL_CLANG_VERSION}/clang+llvm-${DL_CLANG_VERSION}-${CLANG_ARCHIVE}")

FetchContent_GetProperties(CLANG_LOCALINSTALL
  POPULATED CLANG_LOCALINSTALL_POPULATED)
if(NOT CLANG_LOCALINSTALL_POPULATED)
  FetchContent_Populate(CLANG_LOCALINSTALL)
  message(STATUS "Downloaded new clang")
endif()

find_program(Clang_EXECUTABLE
  clang
  PATHS ${CMAKE_SOURCE_DIR}/stm32-tools/clang/${CMAKE_HOST_SYSTEM_NAME}
  PATH_SUFFIXES bin)

find_program(Clang_CLANGFORMAT_EXECUTABLE
  clang-format
  PATHS ${CMAKE_SOURCE_DIR}/stm32-tools/clang/${CMAKE_HOST_SYSTEM_NAME}
  PATH_SUFFIXES bin
  NO_DEFAULT_PATH)

find_program(Clang_CLANGTIDY_EXECUTABLE
  clang-tidy
  PATHS ${CMAKE_SOURCE_DIR}/stm32-tools/clang/${CMAKE_HOST_SYSTEM_NAME}
  PATH_SUFFIXES bin)

find_program(Clang_CODECHECKER_EXECUTABLE
  CodeChecker
  PATHS ${CMAKE_SOURCE_DIR}/stm32-tools/clang/${CMAKE_HOST_SYSTEM_NAME})

if (Clang_CODECHECKER_EXECUTABLE STREQUAL "Clang_CODECHECKER_EXECUTABLE_NOTFOUND")
  message(WARNING "Could not find codechecker, which is system-dependent. See https://codechecker.readthedocs.io/en/latest/#install-guide")
endif()

if(NOT Clang_EXECUTABLE STREQUAL "Clang_EXECUTABLE-NOTFOUND")
  execute_process(
    COMMAND ${Clang_EXECUTABLE} --version
    OUTPUT_VARIABLE INSTALLED_CLANG_VERSION_BLOB)
  string(REGEX MATCH "clang version ([0-9]+\.[0-9]+\.[0-9]+)"
    CV_REGEX_OUTPUT ${INSTALLED_CLANG_VERSION_BLOB})
  # cmake uses temporarily-valid globals for regex subexpression matching for some reason
  # so we use that instead of the output variable from string()
  set(INSTALLED_CLANG_VERSION ${CMAKE_MATCH_1})
  message(STATUS "Found installed clang with version: ${INSTALLED_CLANG_VERSION}")
else()
  message(STATUS "Couldn't find installed clang")
  set(INSTALLED_CLANG_VERSION "0.0.0")
endif()

if (${INSTALLED_CLANG_VERSION} VERSION_LESS ${DL_CLANG_VERSION})
    if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    execute_process("${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}/clang+llvm-${DL_CLANG_VERSION}-${CLANG_ARCHIVE}")
    find_program(Clang_EXECUTABLE
      clang
      REQUIRED)
    find_program(Clang_CLANGTIDY_EXECUTABLE
      clang-tidy
      REQUIRED)
    find_program(Clang_CLANGFORMAT_EXECUTABLE
      clang-format
      REQUIRED)
  else()
    unset(Clang_EXECUTABLE CACHE)
    find_program(Clang_EXECUTABLE clang
      PATHS "${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
      PATH_SUFFIXES bin
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
      REQUIRED)
    unset(Clang_CLANGTIDY_EXECUTABLE CACHE)
    find_program(Clang_CLANGTIDY_EXECUTABLE
      clang-tidy
      PATHS "${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
      PATH_SUFFIXES bin
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
      REQUIRED)
    unset(Clang_CLANGFORMAT_EXECUTABLE CACHE)
    find_program(Clang_CLANGFORMAT_EXECUTABLE
      clang-format
      PATHS "${LOCALINSTALL_CLANG_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
      PATH_SUFFIXES bin
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
      REQUIRED)
    message(STATUS "Found downloaded clang-format: ${Clang_CLANGFORMAT_EXECUTABLE}")
  endif()
endif()

message(STATUS "Found clang-tidy at: ${Clang_CLANGTIDY_EXECUTABLE}")
message(STATUS "Found clang-format at ${Clang_CLANGFORMAT_EXECUTABLE}")
