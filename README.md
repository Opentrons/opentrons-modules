# Opentrons Modules

This repository contains:
1. Firmware for all Opentrons modules - Temperature Module (Tempdeck),
Magnetic Module (Magdeck) and Thermocycler.
2. Libraries required to build any module firmware
3. Module board file definitions for Arduino
4. Other files required for development & testing

## setup

First, clone the repository and change into the directory:

```
git clone git@github.com:Opentrons/opentrons-modules.git
cd opentrons-modules
```

Follow the steps below based on your OS..

**MacOS/ Linux setup:**

**_WARNING: If you already have an Arduino IDE installed, please make sure to close it before
running this step._**

To install Arduino IDE and the required library & board files, open a terminal,
cd into `opentrons-modules/` and simply enter `make setup`
This should implement the following:
- Installing Arduino IDE at `$HOME/arduino_ide`
- Installing board definition files required by all the modules (Opentrons Magdeck,
  Opentrons Tempdeck, Adafruit SAMD, Arduino SAMD)
- Setting up the sketchbook/ library path
- Adding build path at `/opentrons-modules/build` to save the builds

**Windows setup:**

(Windows setup is not implement in `make` yet. So we will have to install the required files manually)

1. Install Arduino IDE (v1.8.5) from https://www.arduino.cc/en/Main/OldSoftwareReleases#previous
2. Install board files:
   - Go to _File->Preferences_ and in _'Additional Boards Manager URLs'_ paste these
   two urls (comma separated):
`https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`,
`https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json`
   - Go to _Tools->Boards_ and select _'Board Manager..'_. Search for and install the
   latest versions of the following:
     - Arduino SAMD Boards
     - Adafruit SAMD Boards
     - Opentrons Modules Boards
3. Set up preferences:
Go to _File->Preferences_ and change _'Sketchbook Location'_ to your `opentrons-modules` repository path

## build

**MacOS/ Linux:**
To build a module firmware, run `make build-<module_name>`.
eg., `make build-tempdeck` for tempdeck, `make build-thermocycler` for thermocycler.

`make build` builds all the modules

* Internal dev stuff:
to build for a dummy board, use `make build-thermocycler DUMMY_BOARD=true`.
This will add the DUMMY_BOARD preprocessor option to your compiler flags. To compile
for the actual module board, remember to change the flag back by building without
the DUMMY_BOARD argument.

The latest build files would be saved in `/opentrons-modules/build/tmp`.
Also, latest .hex/.bin files for the last build of a particular board will be saved
in the build directory under its module name eg. `../build/thermo-cycler/thermo-cycler-arduino.ino.bin`.

**Windows:**
Use Arduino IDE to compile:
Select the correct board file for your project from _Tools->board_
(currently, the thermocycler project uses 'Adafruit Feather M0' board).
Then select _Sketch->Compile_.

The build will be saved in a windows temp directory. You can check verbose compilation
output for the temp directory path. To turn ON verbose mode, go to _File->Preferences_
and select _compilation_ option for _'Show verbose output during'_

## upload

Not implemented in make.
Use the Arduino IDE installed to upload the firmware file:

Open the `/modules/.../<file_name>.ino` file in Arduino, select the appropriate board from _Tools->board_,
 select the correct port from _Tools->Port_. Select Upload or _Sketch->Upload_
