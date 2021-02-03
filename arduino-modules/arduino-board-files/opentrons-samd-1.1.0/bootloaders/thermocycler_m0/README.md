This is an Opentrons Thermocycler M0 compilation of Atmel's SAM-BA bootloader

The src files are located in ../zero. Please refer to zero's README regarding
using Make for compiling your own bootloader. To compile for Thermocycler run:
`BOARD_ID=opentrons_thermocyclerM0 NAME=samd21_sam_ba_thermocyclerM0 make clean all`

To upload the bootloader onto a board using Atmel Studio, you can check instructions with graphics here: https://learn.adafruit.com/how-to-program-samd-bootloaders/programming-the-bootloader-with-atmel-studio (use the bootloader provided in the current directory)

Here's the outline:
1. Open Atmel studio. Plug in your Jlink and connect it to the SWD pins.
2. Go to Tools -> Device Programming.
3. In the Device Programming window, select the Tool as J-Link (you should have J-Link option
  in the dropdown menu). Select ATSAMD21G18A as the device, SWD as the interface and hit Apply.
  You should then be able to Read the Device Signature. Make sure this all works before you continue!
  If you are asked to update the J-Link firmware, its OK to do so now.
4. Go to Fuses on the left hand side and set the `BOOTPROT` fuse to 0x07 i.e. 0 bytes
  (It would be called NVMCTRL_BOOTPROT)
5. Go to Memories in the left hand side. Next to the Flash (256 KB) section, click the triple-
  dots and select the .bin bootloader file from this directory. Select "Erase Flash before programming and Verify flash after programming". Click Program
6. After flashing, you will need to set the BOOTPROT fuse to an 8kB bootloader size.
  From Fuses, set BOOTPROT to 0x02 (i.e 8kb) and click Program
