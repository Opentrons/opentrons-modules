# Thermocycler Refresh Firmware

This directory has the code for the thermocycler gen2 (TC2)

## Basic project structure

The code for this module consists of a core set of code that can be compiled for any target platform, as well as supporting code that implements the lower levels of control for each specific platform. With this structure, the code can be compiled for three primary targets:

- __firmware__ - the actual firmware running on the microcontroller
- __tests__ - code that is intended to work alongside a suite of unit tests
- __simulator__ - a version of the firmware that runs on the host platform (whatever computer is used to compile it), and which simulates the low level behavior of the firmware.

The structure of each target is shown below (arrows indicate dependencies):

'''mermaid
flowchart BT
    A[Simulator Executable]
    B[Thermocycler common code]
    C[Firmware Binary]
    D[Test Suite Executable]
    E[Common libraries]

    F[Simulator-specific code]
    G[Test-specific code]
    H[Firmware-specific code]

    D ---> B
    C ---> B
    A ---> B
    B --> E
    C --> H
    D --> G
    A --> F
'''

## Relevant build system targets

### Cross-build
When cross-compiling the firmware (using the `stm32-cross` cmake preset, running `cmake --build ./build-stm32-cross`), you can
- Build the firmware: `cmake --build ./build-stm32-cross --target thermocycler-gen2`
- Build the firmware, connect to a debugger, and upload it: `cmake --build ./build-stm32-cross --target thermocycler-gen2-debug`
- Lint the firmware: `cmake --build ./build-stm32-cross --target thermocycler-gen2-lint`
- Format the firmware: `cmake --build ./build-stm32-cross --target thermocycler-gen2-format`
- Flash the firmware to a board: `cmake --build ./build-stm32-cross --target thermocycler-gen2-flash`
- Build a .hex file suitable for use with stm's programmer: `cmake --build ./build-stm32-cross --target thermocycler-gen2-hex`
- Build a .bin file suitable for some other programmers: `cmake --build ./build-stm32-cross --target thermocycler-gen2-bin`

### Debugging
There's a target called `thermocycler-gen2-debug` that will build the firmware and then spin up a gdb, spin up an openocd, and connect the two; load some useful python scripts; connect to an st-link that should be already plugged in; automatically upload the firmware, and drop you at a breakpoint at boot time. This should all download itself and be ready as soon as `cmake --preset=stm32-cross .` completes, with one exception: Gdb python support is incredibly weird and will somehow always find your python2 that the system has, no matter how hard you try to avoid this. The scripts should work fine, but you have to install setuptools so `pkg_resources` is available, since this isn't really something we want to "install" by downloading it to some random directory and dropping it in gdb's embedded python interpreter's package path, so do the lovely

`wget https://bootstrap.pypa.io/pip/2.7/get-pip.py`
`python2.7 ./get-pip.py`

You do not have to sudo this, and should not sudo it, it works perfectly fine as a user install.

Once you spin this up, it provides peripheral register names and access through https://github.com/1udo6arre/svd-tools.
That means you can do stuff like 
`help svd` - get info on svd commands (you don't have to do the `svd /path/to/svd`, that's done automatically)
`svd get nvic` - print all the nvic registers

### Host-build
When compiling the firmware using your local compiler (using the `stm32-host` cmake preset, running `cmake --build ./build-stm32-host`), you can
- Build tests: `cmake --build ./build-stm32-host --target thermocycler-gen2-tests`
- Run tests: `cmake --build ./build-stm32-host --target test`
- Format tests: `cmake --build ./build-stm32-test --target thermocycler-gen2-format`
- Build simulator: `cmake --build ./build-stm32-host --target thermocycler-gen2-simulator` 
- Build and Test: `cmake --build ./build-stm32-host --target thermocycler-gen2-build-and-test` 

### Simulator
There's a simulator! It host-compiles the core lib with boost for interaction. It can run with input from either `stdin` or a socket. You can build it with `cmake --build ./build-stm32-host --target thermocycler-gen2-simulator` and then run it with `./build-stm32-host/stm32-modules/thermocycler-gen2/simulator/thermocycler-gen2-simulator --stdin`. You can type some gcodes in to stdin. To quit, either interrupt or kill or send EOF (ctrl-d on unixlikes). See `./simulator` for more detail.

## File Structure
- `./tests/` contains the test-specific entrypoints and actual test code
- `./firmware` contains the code that only runs on the device itself
- `./simulator` contains the code that runs a local simulator
- `./src` contains the code that can be either cross- or host-compiled, and therefore can and should be tested

## Style

We enforce style with [clang-format](https://clang.llvm.org/docs/ClangFormat.html), using the [google C++ style](https://google.github.io/styleguide/cppguide.html). We lint using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/). The configurations for these tools are in the root of the repo (they're hidden files on Unix-likes, `.clang-tidy` and `.clang-format`). 

We use [Catch2](https://github.com/catchorg/Catch2) for testing.
