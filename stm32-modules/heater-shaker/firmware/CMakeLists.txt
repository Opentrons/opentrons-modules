# This CMakeLists.txt handles everything that is compiled only when
# cross-compiling, like the board support packages and special options.

find_package(STM32F303BSP)

add_executable(heater-shaker-main
  main.cpp)
target_link_libraries(heater-shaker-main STM32F303BSP)
target_include_directories(STM32F303BSP
  PUBLIC .)
target_compile_definitions(STM32F303BSP
  PUBLIC STM32F303xE)
