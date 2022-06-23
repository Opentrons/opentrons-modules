#[=======================================================================[.rst:

FindMpalandPrintf.cmake

Download the mpaland/printf repository from Github. This is an implementation
of the standard printf/sprintf family of functions without any dynamic memory
allocation.

To use this library, call `find_package(MpalandPrintf)` from your CMakeLists
and then link your executable to the library `Mpaland_Printf_Library`.

In your code, #include printf.h to get the header.

NOTE: somewhere in your code, you should include a definition of the 
_putchar function declared in printf.h.

#]=======================================================================]

Include(FetchContent)

FetchContent_Declare(
    MpalandPrintf
    GIT_REPOSITORY "https://github.com/mpaland/printf"
    GIT_TAG        "v4.0.0"
    PREFIX         ${CMAKE_SOURCE_DIR}/stm32-tools/
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/printf
)

FetchContent_MakeAvailable(MpalandPrintf)

FetchContent_GetProperties(MpalandPrintf
    POPULATED MpalandPrintf_POPULATED
    SOURCE_DIR MpalandPrintf_SOURCE_DIR
)

set(PRINTF_FOUND ${MpalandPrintf_POPULATED} PARENT_SCOPE)
set(PRINTF_DIRECTORY ${MpalandPrintf_SOURCE_DIR}/ PARENT_SCOPE)

file(GLOB_RECURSE printf_sources ${MpalandPrintf_SOURCE_DIR}/*.c)

add_library(
    Mpaland_Printf_Library STATIC ${printf_sources})

set_target_properties(
    Mpaland_Printf_Library
    PROPERTIES  C_STANDARD 11
                C_STANDARD_REQUIRED TRUE)
                
target_include_directories(
    Mpaland_Printf_Library PUBLIC 
    ${MpalandPrintf_SOURCE_DIR}
)
