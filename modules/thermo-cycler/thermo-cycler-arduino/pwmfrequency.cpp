#include "Pwmfrequency.h"

Pwmfrequency::Pwmfrequency(){}

void Pwmfrequency::pwm_with_frequency(int pin, double duty, double freq_k_hz) {

  if (
    pin != ALLOWED_PIN_TWO ||
    pin != ALLOWED_PIN_FIVE ||
    pin != ALLOWED_PIN_SIX ||
    pin != ALLOWED_PIN_SEVEN) {
    return;
  }

  REG_GCLK_GENDIV = GCLK_GENDIV_DIV(1) |          // Divide the 48MHz clock source by divisor 1: 48MHz/1=48MHz
                    GCLK_GENDIV_ID(4);            // Select Generic Clock (GCLK) 4
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |           // Set the duty cycle to 50/50 HIGH/LOW
                     GCLK_GENCTRL_GENEN |         // Enable GCLK4
                     GCLK_GENCTRL_SRC_DFLL48M |   // Set the 48MHz clock source
                     GCLK_GENCTRL_ID(4);          // Select GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  // Enable the port multiplexer for the digital pin
  PORT->Group[g_APinDescription[pin].ulPort].PINCFG[g_APinDescription[pin].ulPin].bit.PMUXEN = 1;
  
  // Connect the TCC0 timer to digital - port pins are paired odd PMUXO and even PMUXE
  // F & E specify the timers: TCC0, TCC1 and TCC2
  if (pin % 2 == 0) {
    PORT->Group[g_APinDescription[pin].ulPort].PMUX[g_APinDescription[pin].ulPin >> 1].reg = PORT_PMUX_PMUXE_F;
  }
  else {
    PORT->Group[g_APinDescription[pin].ulPort].PMUX[g_APinDescription[pin].ulPin >> 1].reg = PORT_PMUX_PMUXO_F;
  }

  // Feed GCLK4 to TCC0 and TCC1
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN |         // Enable GCLK4 to TCC0 and TCC1
                     GCLK_CLKCTRL_GEN_GCLK4 |     // Select GCLK4
                     GCLK_CLKCTRL_ID_TCC0_TCC1;   // Feed GCLK4 to TCC0 and TCC1
  while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

  // Dual slope PWM operation: timers countinuously count up to PER register value then down 0
  REG_TCC0_WAVE |= TCC_WAVE_POL(0xF) |         // Reverse the output polarity on all TCC0 outputs
                    TCC_WAVE_WAVEGEN_DSBOTH;    // Setup dual slope PWM on TCC0
  while (TCC0->SYNCBUSY.bit.WAVE);               // Wait for synchronization

  // Each timer counts up to a maximum or TOP value set by the PER register,
  // this determines the frequency of the PWM operation:
  double num_ticks_per_duty = 48000 / (freq_k_hz * 2);
  REG_TCC0_PER = num_ticks_per_duty;         // Set the frequency of the PWM on TCC0
  while (TCC0->SYNCBUSY.bit.PER);                // Wait for synchronization
  
  // Set the PWM signal to output 50% duty cycle
  if (pin == ALLOWED_PIN_SEVEN) {
    REG_TCC0_CC3 = uint8_t(duty * num_ticks_per_duty);         // TCC0 CC3 - on D7
    while (TCC0->SYNCBUSY.bit.CC3);                // Wait for synchronization
  }
  else if (pin == ALLOWED_PIN_SIX) {
    REG_TCC0_CC2 = uint8_t(duty * num_ticks_per_duty);         // TCC0 CC3 - on D6
    while (TCC0->SYNCBUSY.bit.CC2);                // Wait for synchronization
  }
  else if (pin == ALLOWED_PIN_FIVE) {
    REG_TCC0_CC1 = uint8_t(duty * num_ticks_per_duty);         // TCC0 CC3 - on D5
    while (TCC0->SYNCBUSY.bit.CC1);                // Wait for synchronization
  }
  else if (pin == ALLOWED_PIN_TWO) {
    REG_TCC0_CC0 = uint8_t(duty * num_ticks_per_duty);         // TCC0 CC3 - on D2
    while (TCC0->SYNCBUSY.bit.CC0);                // Wait for synchronization
  }
  
  // Divide the 48MHz signal by 1 giving 48MHz (20.83ns) TCC0 timer tick and enable the outputs
  REG_TCC0_CTRLA |= TCC_CTRLA_PRESCALER_DIV1 |    // Divide GCLK4 by 1
                    TCC_CTRLA_ENABLE;             // Enable the TCC0 output
  while (TCC0->SYNCBUSY.bit.ENABLE);              // Wait for synchronization
}
