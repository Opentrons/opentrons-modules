#ifndef STARTUP_JUMPS_H_
#define STARTUP_JUMPS_H_

// Called to jump to the bootloader
void jump_to_bootloader(void);
// Called to jump to the main app, if it's valid
void jump_to_application(void);
// Called by the interrupt handlers
void jump_from_exception(void) __attribute__((naked));

#endif /* STARTUP_JUMPS_H_ */
