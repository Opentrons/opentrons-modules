# Flex-Stacker Firmware

This directory has the code for the Flex-Stacker.

## Relevant build system targets

### Cross-build
When cross-compiling the firmware (using the `stm32-cross` cmake preset, running `cmake --build ./build-stm32-cross`), you can
- Build the firmware: `cmake --build ./build-stm32-cross --target flex-stacker`
- Build the firmware, connect to a debugger, and upload it: `cmake --build ./build-stm32-cross --target flex-stacker-debug`
- Lint the firmware: `cmake --build ./build-stm32-cross --target flex-stacker-lint`
- Format the firmware: `cmake --build ./build-stm32-cross --target flex-stacker-format`
- Flash the firmware to a board: `cmake --build ./build-stm32-cross --target flex-stacker-flash`
- Flash the firmware __and__ the startup application to a board: `cmake --build ./build-stm32-cross --target flex-stacker-image-flash`
- Builds flex-stacker-image.hex, suitable for use with stm's programmer: `cmake --build ./build-stm32-cross --target flex-stacker-image-hex`
- Builds flex-stacker-image.bin, suitable for some other programmers: `cmake --build ./build-stm32-cross --target flex-stacker-image-bin`
- Build the startup app, which is also packaged into the image files: `cmake --build ./build-stm32-cross --target flex-stacker-startup`
- Delete all of the contents on a flex-stacker MCU, and wipe any memory protection: `cmake --build ./build-stm32-cross --target flex-stacker-clear`

The default board revision is currently a1 (EVT). You can change the revision to the NFF board by running cmake `stm32-cross` preset again with
`cmake --preset stm32-cross -DSTACKER_REVISION=nff .`
before re-building the firmware.

### Debugging
There's a target called `flex-stacker-debug` that will build the firmware and then spin up a gdb, spin up an openocd, and connect the two; load some useful python scripts; connect to an st-link that should be already plugged in; automatically upload the firmware, and drop you at a breakpoint at boot time. This should all download itself and be ready as soon as `cmake --preset=stm32-cross .` completes, with one exception: Gdb python support is incredibly weird and will somehow always find your python2 that the system has, no matter how hard you try to avoid this. The scripts should work fine, but you have to install setuptools so `pkg_resources` is available, since this isn't really something we want to "install" by downloading it to some random directory and dropping it in gdb's embedded python interpreter's package path, so do the lovely

`wget https://bootstrap.pypa.io/pip/2.7/get-pip.py`
`python2.7 ./get-pip.py`

You do not have to sudo this, and should not sudo it, it works perfectly fine as a user install.

Once you spin this up, it provides peripheral register names and access through https://github.com/1udo6arre/svd-tools.
That means you can do stuff like 
`help svd` - get info on svd commands (you don't have to do the `svd /path/to/svd`, that's done automatically)
`svd get nvic` - print all the nvic registers

### Host-build
When compiling the firmware using your local compiler (using the `stm32-host` cmake preset, running `cmake --build ./build-stm32-host`), you can
- Build tests: `cmake --build ./build-stm32-host --target flex-stacker-tests`
- Run tests: `cmake --build ./build-stm32-host --target test`
- Format tests: `cmake --build ./build-stm32-host --target flex-stacker-format`
- Build simulator: `cmake --build ./build-stm32-host --target flex-stacker-simulator` 
- Build and Test: `cmake --build ./build-stm32-host --target flex-stacker-build-and-test` 

## File Structure
- `./tests/` contains the test-specific entrypoints and actual test code
- `./firmware` contains the code that only runs on the device itself
- `./simulator` contains the code that runs a local simulator
- `./src` contains the code that can be either cross- or host-compiled, and therefore can and should be tested

## Style

We enforce style with [clang-format](https://clang.llvm.org/docs/ClangFormat.html), using the [google C++ style](https://google.github.io/styleguide/cppguide.html). We lint using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/). The configurations for these tools are in the root of the repo (they're hidden files on Unix-likes, `.clang-tidy` and `.clang-format`). 

We use [Catch2](https://github.com/catchorg/Catch2) for testing.
