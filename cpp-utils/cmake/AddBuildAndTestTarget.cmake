#[=======================================================================[.rst:
AddBuildAndTestTarget.cmake
--------------------

This module works around weirdnesses in cmake's test infrastructure.

CMake provides a ``test`` make target when you add tests, which is handy. But
it's a special target that doesn't depend on builds, and because it's a
special target you can't mark the builds as a dependency of the tests.

There is ongoing discussion about this here:
https://gitlab.kitware.com/cmake/cmake/-/issues/8774 which has been ongoing
for about 10 years.

Basically, the cmake developers' views are that running tests shouldn't
depend on the test executable, for reasons I don't quite understand.

Furthermore, because of the ways that Catch2 test dependencies work, you can't
do the workaround occasionally suggested to add a "test" that really builds
the test executable (and this is fairly awful anyway, since you get the results
  of the build polluting your test log).

The workaround done here is to add a custom target called ``build-and-test`` which
subprocesses cmake's test command and is a real target so it can depend on the
build. If you want to run ``cmake --build --target test`` repeatedly, it won't
rebuild anything.

Since the macro this provides needs to know about names, it also actually does
the test integration itself.

ADD_BUILD_AND_TEST_TARGET
^^^^^^^^^^^^^^^^

``ADD_BUILD_AND_TEST_TARGET`` provides infrastructure for tests to run on a
specified target.

Input Variables
+++++++++++++++

``TEST_EXECUTABLE``
A mandatory positional argument that is the test executable target (i.e. the
  first argument to a prior call to ``add_executable``)

Result Variables
++++++++++++++++
This will define a target called ``${TEST_EXECUTABLE}_check`` and add it as
a dependency to the ``check`` target.
#]=======================================================================]


macro(ADD_BUILD_AND_TEST_TARGET TEST_EXECUTABLE)
  add_custom_target(${TEST_EXECUTABLE}-build-and-test
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    DEPENDS ${TEST_EXECUTABLE})
  add_dependencies(build-and-test ${TEST_EXECUTABLE}-build-and-test)
endmacro()
