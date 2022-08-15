#ifndef STARTUP_CHECKS_H_
#define STARTUP_CHECKS_H_

#include <stdbool.h>

/**
 * Checks that:
 *   - The vector table exists (not all 1's)
 *   - The reset vector points to a valid arm thumb instruction
 */
bool check_app_exists();

/**
 * Checks that:
 *   - The Device Integrity region has the correct starting address
 *   - The CRC in the Device Integrity region is correct
 */
bool check_crc();

#endif /* STARTUP_CHECKS_H_ */
