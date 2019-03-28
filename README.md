# Opentrons Modules Dev

This repository contains module firmware and supporting files.

## setup

Clone the repository, change into the directory, and install development dependencies:

**MacOS/ Linux setup:**

**_WARNING: If you already have an Arduino IDE installed, please make sure to close it before
running this step._**

To install Arduino IDE and the required library & board files, simply run `make setup` from `/opentrons-modules`
This should implement the following steps:
- Installing Arduino IDE at `$HOME/arduino_ide`
- Installing board definition files required by all the modules (Opentrons Magdeck, Opentrons Tempdeck, Adafruit SAMD, Arduino SAMD)
- Setting up the sketchbook/ library path
- Adding build path at `/opentrons-modules/build` to save the builds

**Windows setup:**

(No `make` support for Windows yet)

1. Install Arduino IDE (v1.8.5) from https://www.arduino.cc/en/Main/OldSoftwareReleases#previous
2. Install board files:
Go to File->Preferences and in 'Additional Boards Manager URLs' paste these two urls (comma separated):
`https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`,
`https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json`
- Go to Tools->Boards and select 'Board Manager..'. Search for and install the latest versions of the following:
  - Arduino SAMD Boards
  - Adafruit SAMD Boards
  - Opentrons Modules Boards
3. Set up preferences:
Go to File->Preferences and change 'Sketchbook Location' to `opentrons-modules` path

## build

**MacOS/ Linux:**
To build a module firmware, run `make build-<module_name>`.
eg., `make build-tempdeck`, `make build-thermocycler`.
`make build` builds all the modules

* Internal dev stuff:
to build for a dummy board, use `make build-thermocycler DUMMY_BOARD=true`.
This will add the DUMMY_BOARD preprocessor option to your compiler flags. To compile
for the actual module board, remember to change the flag back by building without
the DUMMY_BOARD argument.

The latest build files would be saved in `/opentrons-modules/build/tmp`.
Also, latest .hex/.bin files for the last build of a particular board will be saved in the build directory under its module name eg. `../build/thermo-cycler/thermo-cycler-arduino.ino.bin`.

**Windows:**
Use Arduino IDE to compile. Sketch->Compile.
The build will be saved in a windows temp directory (see verbose compilation output for the path).

## upload
Not implemented in make.
Please use the Arduino IDE installed to upload the firmware file.
[ Open the `/modules/.../<file_name>.ino` file in Arduino, select the appropriate board from Tools->board,
 select the correct port from Tools->Port. Select Upload or Sketch->Upload]
