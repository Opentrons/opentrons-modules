1) Download and install J-Link CLI tool:

https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack

2) Power on device, and connect J-Link to it's SWD pinout

run `JLinkExe` in Terminal
connect over SWD, speed 4000 (default)
defice is `ATSAMD21G18`

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
