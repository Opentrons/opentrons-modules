# This CMakeLists.txt handles compiling all the parts of the module
# that are portable between host and cross compilation as a static
# library. It is included in both host and cross configurations.
# TARGET_MODULE_NAME is set by parent dir to specify module

find_package(Python)
find_package(Git QUIET)

add_custom_command(
  COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/generate_thermistor_table.py
    ${CMAKE_CURRENT_SOURCE_DIR}/ntcg104ed104dtdsx.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/ks103j2.csv
    "BOTH"
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_thermistor_table.py
          ${CMAKE_CURRENT_SOURCE_DIR}/ntcg104ed104dtdsx.csv
          ${CMAKE_CURRENT_SOURCE_DIR}/ks103j2.csv
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
         ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

# Configure lintable/nonlintable sources here
set(CORE_LINTABLE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/pid.cpp
  )
set(CORE_NONLINTABLE_SOURCES 
  ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.cpp
  )

add_library(${TARGET_MODULE_NAME}-core STATIC
  ${CORE_LINTABLE_SOURCES}
  ${CORE_NONLINTABLE_SOURCES}
)

set_target_properties(${TARGET_MODULE_NAME}-core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)

target_include_directories(${TARGET_MODULE_NAME}-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
  # We keep the Binary dir private for common so that other projects linking aginst it don't have
  # to worry about collisions between different versions of the thermistor table headers.
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}  # need to add this to get the generated conversion header
  )

target_compile_options(${TARGET_MODULE_NAME}-core
  PRIVATE
  -Wall
  -Werror
  -Weffc++
  -Wreorder
  -Wsign-promo
  -Wextra-semi
  -Wctor-dtor-privacy
  -fno-rtti
  )

# The -lint target lives here because there are no firmware files
# in 'common' to be linted. Only want to expose this for cross
# compilation anyways.
if (${CMAKE_CROSSCOMPILING})

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
  add_custom_target(${TARGET_MODULE_NAME}-lint
    COMMENT "Linting"
    ALL
    COMMAND ${Clang_CLANGTIDY_EXECUTABLE} ${CLANG_EXTRA_ARGS} -p ${CMAKE_BINARY_DIR} ${CORE_LINTABLE_SOURCES} --config=
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/thermistor_lookups.hpp
    )
  
  
endif()
