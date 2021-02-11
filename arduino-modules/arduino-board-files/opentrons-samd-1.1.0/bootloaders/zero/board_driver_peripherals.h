
#ifndef _BOARD_DRIVER_PERIPHERALS_
#define _BOARD_DRIVER_PERIPHERALS_

#include <sam.h>
#include "board_definitions.h"

#if defined(BOARD_FAN_PORT)
inline void fan_init(void) { PORT->Group[BOARD_FAN_PORT].DIRSET.reg = (1<<BOARD_FAN_PIN); }
inline void fan_off(void) { PORT->Group[BOARD_FAN_PORT].OUTCLR.reg = (1<<BOARD_FAN_PIN); }
#else
inline void fan_init(void) { }
inline void fan_off(void) { }
#endif

#if defined(BOARD_PELTIERS_PORT)
inline void peltiers_init(void) { PORT->Group[BOARD_PELTIERS_PORT].DIRSET.reg = (1<<BOARD_PELTIERS_PIN); }
inline void peltiers_off(void) { PORT->Group[BOARD_PELTIERS_PORT].OUTSET.reg = (1<<BOARD_PELTIERS_PIN); }
#else
inline void peltiers_init(void) { }
inline void peltiers_off(void) { }
#endif

#endif // _BOARD_DRIVER_PERIPHERALS_
