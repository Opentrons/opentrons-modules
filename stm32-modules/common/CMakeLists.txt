#common module code source tree

# This variable defines the name of the target for this subfolder.
set(TARGET_MODULE_NAME "common")

add_subdirectory(src)

if (${CMAKE_CROSSCOMPILING})
  add_subdirectory(STM32G491)
  add_subdirectory(STM32F303)
else()
  add_subdirectory(tests)
endif()

file(GLOB_RECURSE ${TARGET_MODULE_NAME}_SOURCES_FOR_FORMAT
  ./*.cpp ./*.hpp ./*.h
  ../include/${TARGET_MODULE_NAME}/*.hpp ../include/${TARGET_MODULE_NAME}/*.h)
list(FILTER ${TARGET_MODULE_NAME}_SOURCES_FOR_FORMAT EXCLUDE REGEX ".*MCSDK.*")

# Targets for formatting. These are here rather than in individual target CMakeLists (e.g.
# the ones in tests/ or firmware/) because they don't have semantic reasoning involved and
# can therefore operate on file globs, unlike lint/static analysis

# Target for use during dev - edits files
add_custom_target(
  ${TARGET_MODULE_NAME}-format
  ALL
  COMMENT "Formatting code"
  COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -i ${${TARGET_MODULE_NAME}_SOURCES_FOR_FORMAT}
  )

# Target for use in ci - warnings are errors, doesn't edit files
add_custom_target(
  ${TARGET_MODULE_NAME}-format-ci
  COMMENT "Checking format"
  COMMAND ${Clang_CLANGFORMAT_EXECUTABLE} -style=file -Werror --ferror-limit=0 -n ${${TARGET_MODULE_NAME}_SOURCES_FOR_FORMAT}
)
