#ifndef h_freq_pwm
#define h_freq_pwm

#include "Arduino.h"

#define GCLK_GEN4_ID (4u)
#define GENDIV_DIVISOR 8  // 48MHz/8 = 6MHz GCLK

void hfq_analogWrite(uint8_t pin, uint8_t value);

void _syncTC_8(Tc* TCx);
void _syncTCC(Tcc* TCCx);
void _syncDAC();

#endif
