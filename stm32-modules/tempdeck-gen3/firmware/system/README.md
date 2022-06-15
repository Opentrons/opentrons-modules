# System Task

The system task control loop runs every time there is a new pending message. The messages are as follows:
- EnterBootloader to jump to the device bootloader
- GetDeviceInfo to read the device information (serial number, version number)
- SetSerial to update the serial number of the board

When moving to the bootloader, the system task sends messages to other tasks to tell them to shut down their hardware before the bootloader can start. These include:
- A messsage to Thermal Control Task to disable the thermal system
- A message to UITask turn off the UI
- A message to Host Comms to disable USB
Once all of these have been acknowledged, the system jumps to the bootloader.
