# Thermocycler Refresh Simulator

This directory has code specific to the TC Gen2 simulator

## Running the simulator

### Selecting an input

The simulator can receive input from either stdin or a socket.

- To use stdin, simply pass the flag `--stdin` when starting the simulator.
- To use a socket, pass the flag `--socket` with a socket address to use.

### Setting the serial number

The simulator can be initialized with a custom serial identifier with an environment variable. If an environment variable called `SERIAL_NUMBER` is found, its value will be set as the device's serial number on initialization.

### Setting the emulation speed

The simulator has two options for emulating thermal & motor data, either __simulated time__ or __real time__.

- In __simulated time__, all behaviors on the system occur _much_ faster than on a real Thermocycler. The response for the thermal & motor systems will still be emulated with simple models, but the rate at which this emulation happens will be nearly instantaneous.
- In __real time__, all behaviors on the system should occur at the same rate they would on a real Thermocycler. This means that thermal ramp rates will be somewhat close to a realistic ramp, and motor movements will take approximately the same time as a real motor movement.

The default mode is __simulated time__. To select __real time__, you can either 1) pass the flag `--realtime` when starting the simulator, or 2) set an environment variable `USE_REALTIME_SIM=True` before starting the simulator.
