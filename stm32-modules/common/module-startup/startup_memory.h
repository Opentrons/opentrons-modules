#ifndef STARTUP_MEMORY_H_
#define STARTUP_MEMORY_H_

#include <stdbool.h>
#include <stdint.h>

/** Lock the startup app (this application) */
bool memory_lock_startup_region();
/** Overwrite the main section with the backup */
bool memory_copy_backup_to_main();
/** Overwrite the backup with the main section */
bool memory_copy_main_to_backup();

#endif /* STARTUP_MEMORY_H_ */
