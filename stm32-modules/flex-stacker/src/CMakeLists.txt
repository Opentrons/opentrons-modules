# This CMakeLists.txt handles compiling all the parts of the module
# that are portable between host and cross compilation as a static
# library. It is included in both host and cross configurations.
# TARGET_MODULE_NAME is set by parent dir to specify module

find_package(Git QUIET)

set(SHARED_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp-utils/src)
# Generate the thermistor table for this project
set(COMMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common/src)


# This is like version for project but works when there aren't any tags yet
execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "${TARGET_MODULE_NAME}*"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE TAGNAME
        OUTPUT_STRIP_TRAILING_WHITESPACE)
if (NOT TAGNAME)
    set(TAGNAME "@(dev)")
endif()

string(REGEX MATCH "@.*$" PREPENDED_VERSION ${TAGNAME})
string(SUBSTRING "${PREPENDED_VERSION}" 1 -1 VERSION)
if (NOT VERSION)
    set(VERSION "(dev)")
endif()

set(ALLOWED_REVISIONS nff a1)
set(DEFAULT_REV a1)
if(DEFINED STACKER_REVISION)
    list(FIND ALLOWED_REVISIONS ${STACKER_REVISION} index)
    if(index EQUAL -1)
        message(FATAL_ERROR "Invalid REVISION value: ${STACKER_REVISION}. Allowed values are: ${ALLOWED_REVISIONS}")
    endif()
else()
    set(STACKER_REVISION ${DEFAULT_REV})
endif()

set(${TARGET_MODULE_NAME}_VERSION "${VERSION}" CACHE STRING "${TARGET_MODULE_NAME} fw version" FORCE)
set(${TARGET_MODULE_NAME}_BOARD_REVISION "${TARGET_MODULE_NAME}-${STACKER_REVISION}" CACHE STRING "${TARGET_MODULE_NAME} board revision" FORCE)
message(STATUS "Building ${TARGET_MODULE_NAME} version: ${${TARGET_MODULE_NAME}_VERSION} for board revision: ${${TARGET_MODULE_NAME}_BOARD_REVISION}")

configure_file(./version.cpp.in ./version.cpp)

option(ALLOW_ASYNC_ERRORS "Select whether asynchronous errors are sent by firmware")

if(ALLOW_ASYNC_ERRORS)
    add_definitions(-DSYSTEM_ALLOW_ASYNC_ERRORS)
endif()

# Configure lintable/nonlintable sources here
set(CORE_LINTABLE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/errors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/motor_utils.cpp
        )
set(CORE_LINTABLE_SOURCES
        ${CORE_LINTABLE_SOURCES}
        PARENT_SCOPE)
set(CORE_NONLINTABLE_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        )

add_library(${TARGET_MODULE_NAME}-core STATIC
        ${CORE_LINTABLE_SOURCES}
        ${CORE_NONLINTABLE_SOURCES}
        )

set_target_properties(${TARGET_MODULE_NAME}-core
        PROPERTIES CXX_STANDARD 20
        CXX_STANDARD_REQUIRED TRUE
        C_STANDARD 11
        C_STANDARD_REQUIRED TRUE)

target_link_libraries(${TARGET_MODULE_NAME}-core PUBLIC common-core)

target_include_directories(${TARGET_MODULE_NAME}-core
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${TARGET_MODULE_NAME}
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}  # need to add this to get the generated conversion header
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp-utils/include/
        )

target_compile_options(${TARGET_MODULE_NAME}-core
        PRIVATE
        -Wall
        -Werror
        -Wimplicit-fallthrough
        $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
        $<$<COMPILE_LANGUAGE:CXX>:-Wreorder>
        $<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
        $<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        )

add_coverage(${TARGET_MODULE_NAME}-core)
