#ifndef STARTUP_CHECKS_H_
#define STARTUP_CHECKS_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_SLOT_MAIN = 0,
    APP_SLOT_BACKUP = 1,
} APP_SLOT_ENUM;

/** Get the starting address of a slot */
uint32_t slot_start_address(APP_SLOT_ENUM slot);

/** Runs every check for a slot */
bool check_slot(APP_SLOT_ENUM slot);

/**
 * Checks that:
 *   - The vector table exists (not all 1's)
 *   - The reset vector points to a valid arm thumb instruction
 */
bool check_app_exists(APP_SLOT_ENUM slot);

/**
 * Checks that:
 *   - The Device Integrity region has the correct starting address
 *   - The CRC in the Device Integrity region is correct
 */
bool check_crc(APP_SLOT_ENUM slot);

/**
 * Checks that:
 *   - The device name exists
 *   - The device name is correct for this module
 */
bool check_name(APP_SLOT_ENUM slot);

/**
 * Checks if the main app and the backup slot are identical. This check
 * assumes that both regions have had their integrity verified.
 */
bool check_backup_matches_main();

#endif /* STARTUP_CHECKS_H_ */
