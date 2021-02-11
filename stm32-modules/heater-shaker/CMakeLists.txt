#Heater/shaker module source tree

add_subdirectory(src)

if (${CMAKE_CROSSCOMPILING})
  add_subdirectory(firmware)
else()
  add_subdirectory(tests)
endif()
