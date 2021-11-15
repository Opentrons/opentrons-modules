#Heater/shaker module source tree

add_subdirectory(src)

if (${CMAKE_CROSSCOMPILING})
  add_subdirectory(firmware)
else()
  add_subdirectory(tests)
  add_subdirectory(simulator)
endif()

file(GLOB_RECURSE HS_SOURCES_FOR_FORMAT 
  ./*.cpp ./*.hpp ./*.h
  ../include/heater-shaker/*.hpp ../include/heater-shaker/*.h)
list(FILTER HS_SOURCES_FOR_FORMAT EXCLUDE REGEX ".*MCSDK.*")

# Targets for formatting. These are here rather than in individual target CMakeLists (e.g.
# the ones in tests/ or firmware/) because they don't have semantic reasoning involved and
# can therefore operate on file globs, unlike lint/static analysis

# Target for use during dev - edits files
add_custom_target(
  heater-shaker-format
  ALL
  COMMENT "Formatting code"
  COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -i ${HS_SOURCES_FOR_FORMAT}
  )

# Target for use in ci - warnings are errors, doesn't edit files
add_custom_target(
  heater-shaker-format-ci
  COMMENT "Checking format"
  COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -Werror --ferror-limit=0 -n ${HS_SOURCES_FOR_FORMAT}
)
