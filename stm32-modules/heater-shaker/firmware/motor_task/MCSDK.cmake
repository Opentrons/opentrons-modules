
set(MCSDK_VERSION 5.4.4)
set(MCSDK_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MCSDK_v${MCSDK_VERSION}/MotorControl")

add_library(_mc_static STATIC IMPORTED)
set_target_properties(_mc_static
  IMPORTED_LOCATION "${MCSDK_LOCATION}/lib/libmc-gcc_M4_NoCCMRAM.lib")

file(GLOB _mcsdk_any_sources "${MCSDK_LOCATION}/MCLib/Any/Src/*.c")
file(GLOB _mcsdk_f3xx_sources "${MCSDK_LOCATION}/MCLib/F3xx/Src/*.c")

add_library(mc STATIC
  ${_mcsdk_any_sources}
  ${_mcsdk_f3xx_sources}
  )
set_target_properties(mc
  PROPERTIES C_STANDARD 11
             C_STANDARD_REQUIRED TRUE)

target_include_directories(mc PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/MCSDK_v5.4.4/MotorControl/MCSDK/MCLib/Any/Inc"
  "${CMAKE_CURRENT_SOURCE_DIR}/MCSDK_v5.4.4/MotorControl/MCSDK/MCLib/F3xx/Inc")
target_link_libraries(mc _mc_static)
