#include <stdint.h>

#include "startup_jumps.h"
#include "startup_checks.h"

uint32_t SystemCoreClock = 8000000;

// SystemInit will be fine without any action
void SystemInit(void) {}

int main() {
    if(!check_app_exists()) {
        jump_to_bootloader();
    }
    
    jump_to_application();

    while(1) {}
}
