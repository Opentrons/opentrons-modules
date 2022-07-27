# CMake 3.19 is the first to include presets
# (https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
cmake_minimum_required(VERSION 3.19)

project("OpentronsCppUtils"
    LANGUAGES CXX C ASM
    DESCRIPTION "Common utility code for C++ projects")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
enable_testing()
add_custom_target(build-and-test)

# For linting/formatting
find_package(Clang)

add_subdirectory(src)
add_subdirectory(tests)

# Get a list of all source/header files
file(GLOB_RECURSE SOURCES_FOR_FORMAT
    src/*.cpp src/*.hpp src/*.h 
    include/*.hpp include/*.h)

# Target for use during dev - edits files
add_custom_target(
    format
    ALL
    COMMENT "Formatting code"
    COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -i ${SOURCES_FOR_FORMAT}
)

# Target for use in ci - warnings are errors, doesn't edit files
add_custom_target(
    format-ci
    COMMENT "Checking format"
    COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -Werror --ferror-limit=0 -n ${SOURCES_FOR_FORMAT}
)
