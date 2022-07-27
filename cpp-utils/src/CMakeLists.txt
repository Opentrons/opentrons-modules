
find_package(Boost 1.71.0)

# Configure lintable/nonlintable sources here
set(CORE_LINTABLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fixed_point.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.cpp
)
set(CORE_NONLINTABLE_SOURCES 
)

add_library(core STATIC
    ${CORE_LINTABLE_SOURCES}
    ${CORE_NONLINTABLE_SOURCES}
)

target_link_libraries(core PUBLIC Boost::boost)

target_compile_definitions(core PUBLIC ENABLE_LOGGING)

set_target_properties(core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)

target_include_directories(core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
)

target_compile_options(core
    PUBLIC
    -Wall
    -Werror
    -Weffc++
    -Wreorder
    -Wsign-promo
    -Wextra-semi
    -Wctor-dtor-privacy
    -fno-rtti
)

# runs clang-tidy https://releases.llvm.org/11.0.1/tools/clang/tools/extra/docs/clang-tidy/index.html
# which is a catch-all static analyzer/linter
# the empty --config= tells clang-tidy to use the .clang-tidy file in the top level
# An odd thing about this target is that it requires the existance of a compiledb, which
# is produced when you build, and may change if you change compilation options, so in a way
# it depends on a build. But we also want to be able to run this when there wasn't a successful
# build, so there's no explicit dependency set.
# This awful transform is required because the implicit includes that gcc knows how to find (e.g. its
# own implementation of the STL) don't get added to the compile db that clang-tidy uses to figure out
# include directories. So we can use the actually fairly cool transform command to turn them all into
# extra-arg invocations and it'll figure it out.
set(CLANG_EXTRA_ARGS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

get_target_property(SRC_INCLUDE_DIRS core INCLUDE_DIRECTORIES)
list(APPEND CLANG_EXTRA_ARGS ${SRC_INCLUDE_DIRS})
list(TRANSFORM CLANG_EXTRA_ARGS PREPEND --extra-arg=-I)
# This helps with clang accepting what GCC accepts around the implementations of the message queue
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-frelaxed-template-template-args")
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-Werror=implicit-fallthrough")
# We must manually specify the stdlib to use because Clang's default is
# ancient and doesn't support features we use
list(APPEND CLANG_EXTRA_ARGS "--extra-arg=-std=gnu++20")

add_custom_target(lint
    ALL
    COMMENT "Linting"
    COMMAND ${Clang_CLANGTIDY_EXECUTABLE} ${CLANG_EXTRA_ARGS} -p ${CMAKE_CURRENT_BINARY_DIR} ${CORE_LINTABLE_SOURCES}
)
