### WARNING: These instructions are for the old bootloader and might not work
### correctly with the new boards & bootloader.
### For latest bootloader and instructions, please refer to: 
### `arduino-board-files` -> `opentrons-samd` -> `bootloaders` -> `thermocycler_m0`

1) Download and install J-Link CLI tool:

https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack

2) Power on device, and connect J-Link to it's SWD pinout

run `JLinkExe` in Terminal
connect over SWD, speed 4000 (default)

3) Reset the device's registers

`loadfile ATSAMD20J_ResetOptionBytes.mot`

4) Flash the USB bootloader

`loadfile bootloader.hex`

5) Reset the device

`reset`

6) Exit JLink, and power cycle device

7) Connect USB cable to computer, and open Arduino

8) Check that the USB port of visible to the laptop

9) Select `Adafruit Feather M0` as target board

10) Upload firmware
