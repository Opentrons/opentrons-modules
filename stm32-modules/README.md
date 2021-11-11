# STM32 Modules Firmware

This subtree contains the firmware for the Opentrons modules that use the STM32. 

All the CMake stuff in here should be done according to https://cliutils.gitlab.io/modern-cmake.

## File Structure
- `./common` contains all of the code common to multiple modules
- `./heater-shaker` contains all of the code specific to the heater/shaker module
- `./thermocycler-refresh` contains all of the code specific to the thermocycler-refreshmodule
- `./include` contains all the headers, sorted by subdirectory - e.g. `include/common/` is where include files common to all modules live. Within each base `include/` directory, files are sorted into further subdirectories based off of their use - for example, `include/heater-shaker/firmware` contains files specific to the heater/shaker cross-compile build. This is done (rather than the reverse, `firmware/include`) so that code can have liens like `#include "firmware/whatever.hpp"`, which makes it apparent in the code itself what domain the header is in.
