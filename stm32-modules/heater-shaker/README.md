# Heater/Shaker Firmware

This directory has the code for the heater/shaker

## Relevant build system targets

### Cross-build
When cross-compiling the firmware (using the `stm32-cross` cmake preset, running `cmake --build ./build-stm32-cross`), you can
- Build the firmware: `cmake --build ./build-stm32-cross --target heater-shaker`
- Build the firmware, connect to a debugger, and upload it: `cmake --build ./build-stm32-cross --target heater-shaker-debug`
- Lint the firmware: `cmake --build ./build-stm32-cross --target heater-shaker-lint`
- Format the firmware: `cmake --build ./build-stm32-cross --target heater-shaker-format`

### Host-build
When compiling the firmware using your local compiler (using the `stm32-host` cmake preset, running `cmake --build ./build-stm32-host`), you can
- Build tests: `cmake --build ./build-stm32-host --target heater-shaker-tests`
- Run tests: `cmake --build ./build-stm32-host --target test`
- Format tests: `cmake --build ./build-stm32-test --target heater-shaker-format`

## File Structure
- `./tests/` contains the test-specific entrypoints and actual test code
- `./firmware` contains the code that only runs on the device itself
- `./src` contains the code that can be either cross- or host-compiled, and therefore can and should be tested
- `./include` contains all the headers, sorted by subdirectory - e.g. `include/firmware/` is where include files for firmware-specific things live. This is done (rather than the reverse, `firmware/include`) so that code can have liens like `#include "firmware/whatever.hpp"`, which makes it apparent in the code itself what domain the header is in.

## Style

We enforce style with [clang-format](https://clang.llvm.org/docs/ClangFormat.html), using the [google C++ style](https://google.github.io/styleguide/cppguide.html). We lint using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/). The configurations for these tools are in the root of the repo (they're hidden files on Unix-likes, `.clang-tidy` and `.clang-format`). 

We use [Catch2](https://github.com/catchorg/Catch2) for testing.
