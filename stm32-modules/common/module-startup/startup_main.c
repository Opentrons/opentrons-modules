#include <stdint.h>

#include "startup_jumps.h"
#include "startup_checks.h"
#include "startup_memory.h"
#include "startup_hal.h"

int main() {
    HardwareInit();


    bool main_app_exists = check_slot(APP_SLOT_MAIN);
    bool backup_app_exists = check_slot(APP_SLOT_BACKUP);
    
    if(!main_app_exists && backup_app_exists) {
        // No main app but backup app = try to recover with backup app
        (void)memory_copy_backup_to_main();
        if(!check_slot(APP_SLOT_MAIN)) {
            // We failed to recover, jump to bootloader
            jump_to_bootloader();
        }
    }
    else if(!main_app_exists) {
        // In this case, we don't have any backup app
        jump_to_bootloader();
    } else {
        // We have a main app, tbd backup
        if(!backup_app_exists || !check_backup_matches_main()) {
            // Try to update the backup. If this fails we boot to the main app
            // anyways.
            (void)memory_copy_main_to_backup();
        }
    }
    
    // Perform this lock at the last minute, after running all of the 
    // other checks. This means that, when debugging & trying to unlock
    // the flash with a debugger, the host has a chance to restart the
    // device and reprogram it without it jumping right into the firmware.
    (void)memory_lock_startup_region();
    jump_to_application();

    while(1) {}
}
