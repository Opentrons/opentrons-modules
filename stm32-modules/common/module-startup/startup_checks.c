#include "startup_checks.h"

#include "startup_hal.h"

#ifndef APPLICATION_START_ADDRESS
#error APPLICATION_START_ADDRESS must be defined
#endif

#define INVALID_ADDR_MASK (0xFFFFFFF0)

bool check_app_exists() {
    const uint32_t boot_address = *(uint32_t*)APPLICATION_START_ADDRESS;
    // If the address is mostly 1's, it is unprogrammed
    if( (boot_address & INVALID_ADDR_MASK) 
          == INVALID_ADDR_MASK) {
        return false;
    }
    // If the first bit of the address is 0, it is not a thumb instruction.
    if( (boot_address & 1) == 0) {
        return false;
    }
    return true;
}
