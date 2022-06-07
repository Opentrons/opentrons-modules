#[=======================================================================[.rst:
FindCrossGDB.cmake
------------------

This module is intended for use with ``find_package`` and should not be imported on
its own.

It will download (if necessary) the appropriate gdb source code and compile to
run as a cross-debugger in stm32-tools/gdb-cross/systemname

Provides the variables
- ``CrossGDB_FOUND``: bool, true if found
- ``CrossGDB_BINDIR``: The path to the binary dir where ``${GCC11_CROSS_TRIPLE}-gdb``
can be found.
- ``CrossGDB_EXECUTABLE``: The path to the binary to run GDB
#]=======================================================================]

include(FetchContent)
include(ProcessorCount)

ProcessorCount(COMPILE_THREADS)

set(GDB_CROSS_DIR  "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/gdb-cross")
set(GDB_SRC_VERSION "12.1" CACHE STRING "GDB debugger source version")
set(TARGET_ARCH "arm-none-eabi")
set(GDB_BIN_DIR "${GDB_CROSS_DIR}/install")
set(EXPAT_DIR "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/expat")
set(GMP_DIR "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/gmp")
set(MPFR_DIR "${CMAKE_CURRENT_LIST_DIR}/../stm32-tools/mpfr")

set(GDB_CONFIG_OPTIONS
    "--prefix=${GDB_BIN_DIR}"
    "--target=${TARGET_ARCH}"
    "--with-expat"
    "--with-libexpat-prefix=${GDB_BIN_DIR}"
    "--with-libgmp-prefix=${GDB_BIN_DIR}"
    "--with-mpfr"
    "--with-libmpfr-prefix={GDB_BIN_DIR}"
    "--with-python"
    "--enable-werror=no"
    "--enable-build-warnings=no")

set(GDB_ARCHIVE_URL "https://ftp.gnu.org/gnu/gdb/gdb-${GDB_SRC_VERSION}.tar.gz")

FetchContent_Declare(
    GDB_CROSS 
    PREFIX ${GDB_CROSS_DIR}
    SOURCE_DIR "${GDB_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    DOWNLOAD_DIR "${GDB_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    URL "${GDB_ARCHIVE_URL}"
)

# Get Expat for xml support
FetchContent_Declare(
    LIBEXPAT
    PREFIX "${EXPAT_DIR}"
    SOURCE_DIR "${EXPAT_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    DOWNLOAD_DIR "${EXPAT_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    URL "https://github.com/libexpat/libexpat/releases/download/R_2_4_8/expat-2.4.8.tar.bz2"
)

# Get GMP for numbers support
FetchContent_Declare(
    LIBGMP
    PREFIX "${GMP_DIR}"
    SOURCE_DIR "${GMP_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    DOWNLOAD_DIR "${GMP_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    URL "https://gmplib.org/download/gmp/gmp-6.2.1.tar.xz"
)

# Get MPFR for float support
FetchContent_Declare(
    LIBMPFR
    PREFIX "${MPFR_DIR}"
    SOURCE_DIR "${MPFR_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    DOWNLOAD_DIR "${MPFR_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
    URL "https://mpfr.loria.fr/mpfr-current/mpfr-4.1.0.tar.xz"
)

macro(FIND_CROSS_GDB REQUIRED)
    find_program(
        CROSS_GDB_EXECUTABLE
        arm-none-eabi-gdb
        PATHS "${GDB_BIN_DIR}/bin"
        ${REQUIRED}
        NO_DEFAULT_PATH
    )
endmacro()

macro(INSTALL_GDB_LIBRARIES)
    FetchContent_Populate(LIBEXPAT)
    # Configure & compile expat
    message(STATUS "Compiling expat...")
    execute_process(
        COMMAND "./configure" "--prefix=${GDB_BIN_DIR}"
        WORKING_DIRECTORY "${EXPAT_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "-j${COMPILE_THREADS}"
        WORKING_DIRECTORY "${EXPAT_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "install"
        WORKING_DIRECTORY "${EXPAT_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )

    FetchContent_Populate(LIBGMP)
    # Configure & compile GMP
    message(STATUS "Compiling GMP...")
    execute_process(
        COMMAND "./configure" "--prefix=${GDB_BIN_DIR}"
        WORKING_DIRECTORY "${GMP_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "-j${COMPILE_THREADS}"
        WORKING_DIRECTORY "${GMP_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "install"
        WORKING_DIRECTORY "${GMP_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )


    FetchContent_Populate(LIBMPFR)
    # Configure & compile MPFR
    message(STATUS "Compiling MPFR...")
    execute_process(
        COMMAND "./configure" "--prefix=${GDB_BIN_DIR}"
        WORKING_DIRECTORY "${MPFR_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "-j${COMPILE_THREADS}"
        WORKING_DIRECTORY "${MPFR_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "install"
        WORKING_DIRECTORY "${MPFR_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
endmacro()

find_cross_gdb("")

if(${CROSS_GDB_EXECUTABLE} STREQUAL "CROSS_GDB_EXECUTABLE-NOTFOUND")
    message(STATUS "Didn't find cross-gdb, downloading")
    make_directory("${GDB_BIN_DIR}")

    install_gdb_libraries()

    # Configure & compile GDB
    FetchContent_Populate(GDB_CROSS)
    message(STATUS "Configuring gdb...")
    execute_process(
        COMMAND "./configure" ${GDB_CONFIG_OPTIONS}
        WORKING_DIRECTORY "${GDB_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    message(STATUS "Compiling gdb (this may take a while)...")
    execute_process(
        COMMAND "make" "all-gdb" "-j${COMPILE_THREADS}"
        WORKING_DIRECTORY "${GDB_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND "make" "install-gdb"
        WORKING_DIRECTORY "${GDB_CROSS_DIR}/${CMAKE_HOST_SYSTEM_NAME}"
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
    )
else()
    message(STATUS "Found cross gdb: ${CROSS_GDB_EXECUTABLE}")  
endif()

find_cross_gdb(REQUIRED)

get_filename_component(CROSS_GDB_BINDIR ${CROSS_GDB_EXECUTABLE} DIRECTORY)

set(CrossGDB_FOUND TRUE)
set(CrossGDB_BINDIR ${CROSS_GDB_BINDIR})
set(CrossGDB_EXECUTABLE ${CROSS_GDB_EXECUTABLE})
