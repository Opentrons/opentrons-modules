#[=======================================================================[.rst:
FindSTM32F303BSP
----------------

Provides a module find backend for the STM32F303 board support package.

This should not be imported. It should be used with find_package:
``find_package(STM32F303BSP)``

When called with find package, it will add a static library target of the same
name, with appropriate definitions, suitable for use in target_link_library.

Callers need to define some further options:
- ``target_compile_definitions`` should be used on the library target to set
the appropriate board definition for the boardfile for the specific processor
being used (i.e. ``target_compile_definitions(STM32F303BSP PUBLIC STM32F303xE)``)
- ``target_include_directories`` should be used to add an include directory that
contains the STM32 hal conf file ``stm32f3xx_hal_conf.h``, which is included by
the library to provide the individual HAL module enable/disable flags

Provides the variables ``STM32F303BSP_FOUND`` as a bool and ``STM32F303BSP_DIR``
as the path to the source.

#]=======================================================================]


include(FetchContent)

FetchContent_Declare(
  STM32_F303_BSP_SOURCE
  GIT_REPOSITORY "https://github.com/STMicroelectronics/STM32CubeF3"
  GIT_TAG "v1.11.2"
  GIT_SHALLOW
  PREFIX ${CMAKE_SOURCE_DIR}/stm32-tools/
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/stm32-tools/stm32f303-bsp
  )

FetchContent_MakeAvailable(STM32_F303_BSP_SOURCE)
FetchContent_GetProperties(STM32_F303_BSP_SOURCE
  SOURCE_DIR bsp_source
  POPULATED bsp_populated
  )

file(GLOB_RECURSE hal_driver_sources ${bsp_source}/Drivers/STM32F3xx_HAL_Driver/Src/*.c)

add_library(STM32F303BSP STATIC
  ${hal_driver_sources})


target_include_directories(
  STM32F303BSP
  PUBLIC ${bsp_source}/Drivers/STM32F3xx_HAL_Driver/Inc
         ${bsp_source}/Drivers/STM32F3xx_HAL_Driver/Inc/Legacy
         ${bsp_source}/Drivers/CMSIS/Device/ST/STM32F3xx/Include
         ${bsp_source}/Drivers/CMSIS/Core/Include
  )

set(STM32F303BSP_FOUND ${bsp_populated} PARENT_SCOPE)
set(STM32F303BSP_DIR ${bsp_source} PARENT_SCOPE)
