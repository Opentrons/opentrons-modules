# Common STM32 Module Code

This directory contains any code shared between multiple STM32 module targets

## Relevant build system targets

### Cross-build
Unlike the other stm32-module subdirectories, there is no direct cross-compile target for the common code; all code is compiled into a library for one of the STM32 compilation targets. The library targets that other modules can link against are:
- common-STM32F303
- common-STM32G491

Other than the MCU-specific libraries, you can run the following targets on the common code:
- Lint the firmware: `cmake --build ./build-stm32-cross --target common-lint`
- Format the firmware: `cmake --build ./build-stm32-cross --target common-format`

### Host-build
When compiling the firmware using your local compiler (using the `stm32-host` cmake preset, running `cmake --build ./build-stm32-host`), you can
- Build tests: `cmake --build ./build-stm32-host --target common-tests`
- Run tests: `cmake --build ./build-stm32-host --target test`
- Format tests: `cmake --build ./build-stm32-test --target common-format`
- Build simulator: `cmake --build ./build-stm32-host --target common-simulator` 
- Build and Test: `cmake --build ./build-stm32-host --target common-build-and-test` 

## File Structure
- `./tests/` contains the test-specific entrypoints and actual test code
- `./simulator` contains the code that runs a local simulator
- `./src` contains the code that can be either cross- or host-compiled, and therefore can and should be tested
- `./STM32F303` contains code and configuration files for running on an STM32F303 MCU
- `./STM32G491` contains code and configuration files for running on an STM32G491 MCU

## Style

We enforce style with [clang-format](https://clang.llvm.org/docs/ClangFormat.html), using the [google C++ style](https://google.github.io/styleguide/cppguide.html). We lint using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/). The configurations for these tools are in the root of the repo (they're hidden files on Unix-likes, `.clang-tidy` and `.clang-format`). 

We use [Catch2](https://github.com/catchorg/Catch2) for testing.
