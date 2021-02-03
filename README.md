# Opentrons Modules

This repository contains:
1. Firmware for all Opentrons modules - Temperature Module (Tempdeck),
Magnetic Module (Magdeck) and Thermocycler.
2. Libraries required to build any module firmware
3. Module board file definitions for Arduino
4. Other files required for development & testing

## setup

This repo uses [CMake](https://cmake.org) as a configuration and build management tool. It requires
CMake 3.19 since it uses [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
to manage cross-compilation and project configuration options.

First, clone the repository and change into the directory:

```
git clone git@github.com:Opentrons/opentrons-modules.git
cd opentrons-modules
```

Then, run `cmake . --preset=arduino` to set your build step up to build the Arduino-based modules
(Magnetic Module, Temperature Module, Thermocycler). This should download the Arduino IDE to the
git-ignored `arduino_ide` directory in the repo's working tree. It will also set up a build system
in `./builds`.

## build

To build a module firmware, run `cmake --build ./build --target <module-name>`, e.g.
`cmake --build ./build --target magdeck`. This will build the sketch and put the sketch and some supporting
files (like a python-based uploader and a hex for writing the eeprom) in `./builds/<module-name>`.

If you run `cmake --install`, all the module firmwares will be built and zipped up with their supporting
files and put in the `dist` directory.

## upload

Not implemented in cmake.
Use the Arduino IDE installed to upload the firmware file:

Open the `/modules/.../<file_name>.ino` file in Arduino, select the appropriate board from _Tools->board_,
 select the correct port from _Tools->Port_. Select Upload or _Sketch->Upload_

* NOTE: For Thermocycler, you can also use the `firmware_uploader.py` script in thermo-cycler/production to upload the binary.
