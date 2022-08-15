#include <stdint.h>

#include "startup_jumps.h"
#include "startup_checks.h"
#include "startup_hal.h"

int main() {
    HardwareInit();
    
    if(!check_app_exists()) {
        jump_to_bootloader();
    }
    if(!check_crc()) {
        jump_to_bootloader();
    }
    
    jump_to_application();

    while(1) {}
}
