# MODULES STARTUP
The STM32 modules use the STM32 inbuilt bootloader which allows for firmware updates via USB through the DFU protocol. In the scenario that a module's power is removed while it is in the process of updating, there is a chance that the firmware loaded on the microcontroller will be invalid. In this scenario, there is no way for the module to reach the bootloader again, and thus it will be "bricked" forever, unable to ever boot back to the bootloader.

The code in this folder is intended to provide a failsafe system to prevent this scenario. The first sector of memory in the micocontroller is occupied by the startup assembly file

https://developer.arm.com/documentation/ka002218/latest
