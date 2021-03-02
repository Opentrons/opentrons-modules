# Opentrons Modules

This repository contains:
1. Firmware for all Opentrons modules - Temperature Module (Tempdeck),
Magnetic Module (Magdeck) and Thermocycler.
2. Libraries required to build any module firmware
3. Module board file definitions for Arduino (for the arduino-based modules)
4. Other files required for development & testing

## setup

This repo uses [CMake](https://cmake.org) as a configuration and build management tool. It requires
CMake 3.19 since it uses [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
to manage cross-compilation and project configuration options. If you want to interact with the STM32 hardware, you should also install [st-link](https://github.com/stlink-org/stlink). 

First, clone the repository and change into the directory:

```
git clone git@github.com:Opentrons/opentrons-modules.git
cd opentrons-modules
```

Then,
- If you want to build the arduino modules, run `cmake --preset=arduino .` to set your build step up to build the Arduino-based modules (Magnetic Module, Temperature Module, Thermocycler). This should download the Arduino IDE to the git-ignored `arduino_ide` directory in the repo's working tree. It will also set up a build system in `./builds`.
- If you want to build the STM32 modules, run `cmake --preset=stm32-cross .` to set your build step up to cross-compile the module firmwares for actually putting on modules. This should download the arm cross builds of gcc 10-2020q4 to the git-ignored `stm32-tools` directory in the repo's working tree and set up a build system in `./build-stm32-cross`. By default, this will be a "Debug" build type, which generates debug symbols packed into the elf and doesn't use optimizations (which makes debugging a lot easier). You can change the build type by adding `-DCMAKE_BUILD_TYPE=<BUILD_TYPE>` on the command line when generating. Valid build types are `Debug` (default), `MinSizeRel` (size optimizations for release - what CI uses to build), and `RelWithDebInfo` (optimizations + debug info, useful if you're seeing failures in the field that don't happen in `Debug` builds).
- If you want to build the STM32 module tests, run `cmake --preset=stm32-host .` to set your build step up to host-compile the module fimrware libs and tests for local running. You'll need to have at least a gcc installed; if you have a clang installed, the build will run some clang checker steps. This will set up a build system in `./build-stm32-host`

Since all these configuration steps use separate build dirs (BINARY_DIRS in cmake parlance) you can in fact run all three of these; it's not exclusive. They should all work on any operating system, but the host builds in particular rely on your system compiler and that will therefore need to be set up, which might be a pain on windows.

## build

Each build dir should have a reasonable `all` target (invokable by running `cmake --build <build-dir>`) and individual targets for each module firmware. For instance, the arduino modules have `<module-name>` set up as individual build targets. 

You always have to specify the build dir to run in with `cmake --build`; that can be run from the root of the repo (e.g. by calling `cmake --build ./build-stm32-cross` or `cmake --build ./build`) or as `.` if you navigate into the right build directory first.

To build a specific target, use the `--target` flag: `cmake --build ./build --target magdeck`.

For the arduino modules, the module name target builds the sketch and copies some supporting files like a python-based uploader and a hex for writing the eeprom into `builds/module-name`. For arduino modules, the special target `zip-all` will build zip files of all module firmwares and support files (e.g. eeprom writing): `cmake --build ./build-arduino --target zip-all`. You can then run `cmake --install ./build` and those zip files will be put in the `dist/` directory; this is used in ci.

For stm32 modules, there are module name targets that build the requisite firmware defined in the cross toolchain and the test toolchain alike. In the cross toolchain, these build runnable firmwares; in the host toolchain, they build everything necessary to run tests, but running the tests is done with `cmake --test`.


## upload

This is not implemented in cmake for the arduino modules. Use the Arduino IDE installed to upload the firmware file:

Open the `/modules/.../<file_name>.ino` file in Arduino, select the appropriate board from _Tools->board_,
 select the correct port from _Tools->Port_. Select Upload or _Sketch->Upload_

* NOTE: For Thermocycler, you can also use the `firmware_uploader.py` script in thermo-cycler/production to upload the binary.

The stm32 modules have debug harnessing built in; for instance, running `cmake --build ./build-stm32-cross --target heater-shaker-debug` will build the firmware and spin up the cross gdb and openocd. This target by default (because of commands in the gdbinit) will reset the board and upload the built firmware; if you want to use the firmware currently flashed to the board, run everything manually.

## test

This is not implemented for the arduino modules.

For the STM32 modules, if you configure the host toolchain: `cmake --preset=stm32-host .`, you can then run the `check` target: `cmake --build ./build-stm32-host --target build-and-test`, which will build the tests and then run them. You can also use cmake's built-in `test` target: `cmake --build ./build-stm32-host --target test`, but this will not automatically actually build the tests.

Individual tests may set their own check targets; for instance, you can build and run only the heater-shaker tests by running `cmake --build ./build-stm32-host --target heater-shaker-build-and-test`.

## Contributing

When writing or changing the cmake build system, please follow [modern CMake](https://cliutils.gitlab.io/modern-cmake) practices. Use targets; use generator expressions to set properties on those targets rather than setting ``CMAKE_DEBUG_FLAGS``; write CMake like the programming language it is.
