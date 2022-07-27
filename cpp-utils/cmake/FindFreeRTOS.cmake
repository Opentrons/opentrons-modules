#[=======================================================================[.rst:

FindFreeRTOS.cmake

Download the FreeRTOS repo from github.

#]=======================================================================]

Include(FetchContent)

FetchContent_Declare(
        FreeRTOS
        GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS
        GIT_TAG        V10.4.0
        PREFIX         ${CMAKE_SOURCE_DIR}/stm32-tools/FreeRTOS
        SOURCE_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/FreeRTOS/src
        BINARY_DIR     ${CMAKE_SOURCE_DIR}/stm32-tools/FreeRTOS/bin
        STAMP_DIR      ${CMAKE_SOURCE_DIR}/stm32-tools/FreeRTOS/stamps
)

FetchContent_MakeAvailable(
        FreeRTOS
)

FetchContent_GetProperties(
        FreeRTOS
        POPULATED FreeRTOS_POPULATED
        SOURCE_DIR FreeRTOS_SOURCE_DIR
)
