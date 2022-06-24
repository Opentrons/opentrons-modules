#[=======================================================================[.rst:
FindBoost
---------

This is a CMake provider for finding the boost (https://www.boost.org/). It
replaces the built-in CMake infrastructure for finding boost because CMake
only wants to find precompiled boosts installed by your system, and we can
instead download boost when necessary and integrate building boost libraries
on their own, as-needed, with the targets that use them instead of forcing
ahead-of-time setup not captured in CMake.

This relies on https://github.com/Orphis/boost-cmake which is subtreed in
to this repo. We need that other project because on its own, boost really
doesn't want to work like this: it wants to be compiled ahead of time, it
wants to be installed in the system. By using boost-cmake, we can integrate
it piecemeal.

The work done by this finder is mostly about wrapping the download locations
and making them go to the same place as all the other stuff this repo
downloads.

Boost should not be used in firmware builds because it is absolutely massive
and most of its functionality is not useful in an embedded context since it
relies on platform or c++ assumptions around things like multithreading or
runtimes; however, it is very useful when building code to operate on host
operating systems (e.g. for simulators).

Usage
+++++

To use this module, make sure you're setting the cmake module path to this
directory and call
```
find_package(Boost VERSION version)
```

The normal cmake built-in find boost has quite a lot of options but because
boost-cmake defers compilation and actually adds the libraries as separate
elements in the build tree, we don't need most of them, and those we do need
are handled in here.

Output Variables
++++++++++++++++

- ``Boost_FOUND`` is a bool that is true if the find-package succeeded
- ``Boost_VERSION`` is the version provided.

Cache Variables
+++++++++++++++
- ``BOOST_SOURCE`` is the path to the source directory


#]=======================================================================]

include(FetchContent)
string(REPLACE "." "_" "_boost_archive_version_component" "${Boost_FIND_VERSION}")
set(boost_localinstall_root ${CMAKE_SOURCE_DIR}/stm32-tools/boost-${_boost_archive_version_component})
set(boost_archive_root "https://boostorg.jfrog.io/artifactory/main/release")
set(boost_archive_url "${boost_archive_root}/${Boost_FIND_VERSION}/source/boost_${_boost_archive_version_component}.zip")
FetchContent_Declare(Boost
  PREFIX "${boost_localinstall_root}"
  SOURCE_DIR "${boost_localinstall_root}/${CMAKE_HOST_SYSTEM_NAME}"
  DOWNLOAD_DIR "${boost_localinstall_root}/${CMAKE_HOST_SYSTEM_NAME}"
  URL "${boost_archive_url}"
)
FetchContent_GetProperties(Boost
    POPULATED Boost_POPULATED
    SOURCE_DIR Boost_SOURCE_DIR)
if (NOT Boost_POPULATED)
  message(STATUS "Fetching boost to stm32-tools")
  FetchContent_Populate(Boost)
  FetchContent_GetProperties(Boost
    POPULATED Boost_POPULATED
    SOURCE_DIR Boost_SOURCE_DIR)
endif()
message(STATUS "Boost downloaded to ${Boost_SOURCE_DIR}")
set(Boost_FOUND ${Boost_POPULATED} PARENT_SCOPE)
set(BOOST_SOURCE ${Boost_SOURCE_DIR} CACHE PATH "Path to where boost was downloaded")
set(Boost_VERSION ${Boost_FIND_VERSION} PARENT_SCOPE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/boost-cmake ${CMAKE_CURRENT_BINARY_DIR}/boost-${_boost_archive_version_component})
