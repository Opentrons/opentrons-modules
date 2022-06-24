#[=======================================================================[.rst:
FindSTM32G4xx
----------------

Provides a module find backend for the STM32G4xx board support package.

This should not be imported. It should be used with find_package:
``find_package(STM32G4xx COMPONENTS BSP FreeRTOS USB )``

When called with find package, it will add a static library target of the same
name, with appropriate definitions, suitable for use in target_link_library.

There are several available subcomponents that callers may want to pick and
choose from; some of them require different supporting setup in the calling
project. Each component is namespaced under STM32G4xx.

Provides the variables ``STM32G4xx_BSP_FOUND`` as a bool and ``STM32G4xx_BSP_DIR``
as the path to the source.

Each component has its own ``STM32G4xx_${COMPONENT}_FOUND`` and so on set
of exported variables.

The ``Drivers`` subcomponent is always selected no matter whether it is present
in the components list, since it is the fundamental part of the BSP.

Drivers
+++++++

The Drivers component (``STM32G4xx_Drivers``) contains the drivers and device
header files. ``find_package`` exported variables are
``STM32G4xx_Drivers_FOUND``, etc. This is the contents of the ``Drivers/``
subdirectory of the BSP.

You can select the Drivers component by adding ``Drivers`` to the ``COMPONENTS``
argument of ``find_package``: ``find_package(STM32G4xx COMPONENTS Drivers)``.
This can then be added to calling projects by calling
``target_link_libraries(mytarget STM32G4xx_Drivers)``.

Some options need to be set by the calling project on the imported
``STM32G4xx_Drivers`` target before it can be used. Specifically,

- ``target_compile_definitions`` should be used on the library target to set
the appropriate board definition for the boardfile for the specific processor
being used (i.e.
``target_compile_definitions(STM32G4xx_Drivers PUBLIC STM32G4xx)``)
- ``target_include_directories`` should be used to add an include directory that
contains the STM32 hal conf file ``stm32g4xx_hal_conf.h``, which is included by
the library to provide the individual HAL module enable/disable flags


FreeRTOS
++++++++

The FreeRTOS component (``STM32G4xx_FreeRTOS``) contains a port of FreeRTOS to
the STM32. This is the ``Middlewares/Third_Party/FreeRTOS`` subdirectory, or at
least most of it (this isn't compiler-sensitive, it always selects the GCC version
 of compiler-dependent options).

You can select the FreeRTOS component by adding ``FreeRTOS`` to the ``COMPONENTS``
argument of ``find_package``: ``find_package(STM32G4xx COMPONENTS FreeRTOS)``.
This can then be added to calling projects by calling
``target_link_libraries(mytarget STM32G4xx_FreeRTOS)``.

Some options need to be set by the calling project on the imported
``STM32G4xx_FreeRTOS`` target before it can be used. Specifically,

- ``target_compile_definitions`` should be used on the library target to set
the appropriate board definition for the boardfile for the specific processor
being used (i.e.
  ``target_compile_definitions(STM32G4xx_FreeRTOS PUBLIC STM32G4xx)``)
- ``target_include_directories`` should be used to add an include directory that
contains a ``FreeRTOSConfig.h`` file that sets FreeRTOS configuration options.
- The caller can also set a CMake property on the imported
``STM32G4xx_FreeRTOS`` component called ``FREERTOS_HEAP_IMPLEMENTATION`` to
select the heap implementation from the ones provided by freertos. These should
be the names of the one of the heap implementation c files without the extension,
e.g.
``set_target_properties(STM32G4xx_FreeRTOS PROPERTIES FREERTOS_HEAP_IMPLEMENTATION "heap_5")``

#]=======================================================================]


include(FetchContent)

FetchContent_Declare(
        STM32_G491RE_BSP_SOURCE
        GIT_REPOSITORY "https://github.com/STMicroelectronics/STM32CubeG4"
        GIT_TAG "v1.4.0"
        GIT_SHALLOW
        PREFIX ${CMAKE_SOURCE_DIR}/stm32-tools/
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/stm32-tools/stm32g4xx-bsp
)

FetchContent_MakeAvailable(STM32_G491RE_BSP_SOURCE)
FetchContent_GetProperties(STM32_G491RE_BSP_SOURCE
        SOURCE_DIR bsp_source
        POPULATED bsp_populated
        )

set(STM32G4xx_BSP_FOUND ${bsp_populated} PARENT_SCOPE)
set(STM32G4xx_BSP_DIRECTORY ${bsp_source} PARENT_SCOPE)
set(STM32G4xx_BSP_VERSION "1.4.0" PARENT_SCOPE)
set(STM32G4xx_BSP_VERSION_STRING "1.4.0" PARENT_SCOPE)

file(GLOB_RECURSE hal_driver_sources ${bsp_source}/Drivers/STM32G4xx_HAL_Driver/Src/*.c)
# GLOB_RECURSE lists files in alphabetical order. That means that the hal, which is
# structured so that each subsystem has stm32g4xx_hal_subsystem.c and
# stm32g4xx_hal_subsystem_ex.c, will have subsystem.c packed into the archive
# before hal_subsystem_ex.c. That breaks some of the hal code, which has weak definitions
# of some functions in subsystem.c that should be overridden in subsystem_ex.c, but
# if subsystem.c comes before subsystem_ex.c in the archive, the linker will pick
# the weak one. By reversing the order, we make subsystem_ex come befeore subsystem,
# and the correct function gets picked.
list(REVERSE hal_driver_sources)

macro(add_STM32G4_driver suffix)
        add_library(
                STM32G4xx_Drivers_${suffix} STATIC
                ${hal_driver_sources})

        target_include_directories(
                STM32G4xx_Drivers_${suffix}
                PUBLIC ${bsp_source}/Drivers/STM32G4xx_HAL_Driver/Inc
                ${bsp_source}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
                ${bsp_source}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
                ${bsp_source}/Drivers/CMSIS/Core/Include)

        set_target_properties(
                STM32G4xx_Drivers_${suffix}
                PROPERTIES C_STANDARD 11
                C_STANDARD_REQUIRED TRUE)
endmacro()
set(STM32G4xx_Drivers_FOUND ${bsp_populated} PARENT_SCOPE)

set(freertos_source ${bsp_source}/Middlewares/Third_Party/FreeRTOS/Source)
set(freertos_port_source ${freertos_source}/portable/GCC/ARM_CM4F)
file(GLOB freertos_common_sources ${freertos_source}/*.c)

macro(add_STM32G4_freertos suffix)
        add_library(
                STM32G4xx_FreeRTOS_${suffix} STATIC
                ${freertos_common_sources}
                ${freertos_port_source}/port.c
                $<IF:$<BOOL:$<TARGET_PROPERTY:FREERTOS_HEAP_IMPLEMENTATION>>,${freertos_source}/portable/MemMang/$<TARGET_PROPERTY:FREERTOS_HEAP_IMPLEMENTATION>.c,"">)

        target_include_directories(
                STM32G4xx_FreeRTOS_${suffix}
                PUBLIC ${freertos_source}/include
                ${freertos_port_source})

        set_target_properties(
                STM32G4xx_FreeRTOS_${suffix}
                PROPERTIES C_STANDARD 11
                C_STANDARD_REQUIRED TRUE)
endmacro()
set(STM32G4xx_FreeRTOS_FOUND ${bsp_populated} PARENT_SCOPE)

macro(add_STM32G4_usb suffix)
        set(usb_root ${bsp_source}/Middlewares/ST/STM32_USB_Device_Library/)
        set(usb_core_root ${usb_root}/Core)
        set(usb_cdc_root ${usb_root}/Class/CDC)
        set(usb_class_sources
                ${usb_cdc_root}/Src/usbd_cdc.c)
        set(usb_core_sources
                ${usb_core_root}/Src/usbd_core.c
                ${usb_core_root}/Src/usbd_ctlreq.c
                ${usb_core_root}/Src/usbd_ioreq.c)

        add_library(
                STM32G4xx_USB_${suffix} STATIC 
                ${usb_core_sources}
                ${usb_class_sources})

        get_target_property(_hal_include_dirs STM32G4xx_Drivers_${suffix} INCLUDE_DIRECTORIES)

        target_include_directories(
                STM32G4xx_USB_${suffix}
                PUBLIC  ${usb_core_root}/Inc
                        ${usb_cdc_root}/Inc
                        ${_hal_include_dirs})
        
        set_target_properties(
                STM32G4xx_USB_${suffix}
                PROPERTIES C_STANDARD 11
                C_STANDARD_REQUIRED TRUE)
endmacro()
set(STM32G4xx_USB_FOUND ${bsp_populated} PARENT_SCOPE)
