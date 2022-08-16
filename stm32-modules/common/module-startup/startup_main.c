#include <stdint.h>

#include "startup_jumps.h"
#include "startup_checks.h"
#include "startup_memory.h"
#include "startup_hal.h"

int main() {
    HardwareInit();

    // Because this lock is performed almost immediately on reset, it becomes
    // impossible in practice to unlock the flash region, even with a
    // debugger attached - the reset will simply occur too quickly and the
    // option bits will get rewritten.
    // 
    // In order to update the startup region, the best option is to set
    // the Read Protection Mode to Level 1, and then back to Level 0. This
    // will clear the entire main flash region, including this startup app,
    // allowing a debugger to clear out the Write Protection bits.
    (void)memory_lock_startup_region();

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
    
    jump_to_application();

    while(1) {}
}
