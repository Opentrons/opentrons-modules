#include "high_frequency_pwm.h"
#include "wiring_private.h"

/* This function mimics Arduino's analogWrite() function (found in wiring_analog.c)
 * in setting the Timer/Counter registers for generating a high frequency (~25kHZ),
 * PWM waveform.
 * ##IMP NOTE:### Do not use analogWrite() and hfq_analogWrite() in the same code.
 * This will result in unpredictable frequencies since the functions will try to
 * overwrite each other's clock configurations. */
void hfq_analogWrite(uint8_t pin, uint8_t value)
{
  PinDescription pinDesc = g_APinDescription[pin];
  uint32_t attr = pinDesc.ulPinAttribute;

 // ATSAMR, for example, doesn't have a DAC
#ifdef DAC
  if ((attr & PIN_ATTR_ANALOG) == PIN_ATTR_ANALOG)
  {
    // DAC handling code
    if (pin == PIN_A0)
    {
      // Only 1 DAC on A0 (PA02)
      // If using DAC, the PWM frequency will differ.
      // But the thermocycler doesn't use the DAC pin as PWM pin so we need not worry about it
			_syncDAC();
			DAC->DATA.reg = value & 0x3FF;  // DAC on 10 bits.
			_syncDAC();
			DAC->CTRLA.bit.ENABLE = 0x01;     // Enable DAC
			_syncDAC();
			return;
    }
  }
#endif // DAC

  if ((attr & PIN_ATTR_PWM) == PIN_ATTR_PWM)
  {
    uint32_t tcNum = GetTCNumber(pinDesc.ulPWMChannel);
    uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);
    static bool tcEnabled[TCC_INST_NUM+TC_INST_NUM];

    value = map(value, 0, 255, 0, 240);

    if (attr & PIN_ATTR_TIMER)
    {
#if !(ARDUINO_SAMD_VARIANT_COMPLIANCE >= 10603)
      // Compatibility for cores based on SAMD core <=1.6.2
      if (pinDesc.ulPinType == PIO_TIMER_ALT)
      {
        pinPeripheral(pin, PIO_TIMER_ALT);
      } else
#endif
      {
        pinPeripheral(pin, PIO_TIMER);
      }
    }
    else if ((attr & PIN_ATTR_TIMER_ALT) == PIN_ATTR_TIMER_ALT)
    {
      //this is on an alt timer
      pinPeripheral(pin, PIO_TIMER_ALT);
    }
    else
    {
      return;
    }

    if (!tcEnabled[tcNum])
    {
      tcEnabled[tcNum] = true;
	    uint16_t GCLK_CLKCTRL_IDs[] = {
    		GCLK_CLKCTRL_ID(GCM_TCC0_TCC1), // TCC0
    		GCLK_CLKCTRL_ID(GCM_TCC0_TCC1), // TCC1
    		GCLK_CLKCTRL_ID(GCM_TCC2_TC3),  // TCC2
    		GCLK_CLKCTRL_ID(GCM_TCC2_TC3),  // TC3
    		GCLK_CLKCTRL_ID(GCM_TC4_TC5),   // TC4
    		GCLK_CLKCTRL_ID(GCM_TC4_TC5),   // TC5
    		GCLK_CLKCTRL_ID(GCM_TC6_TC7),   // TC6
    		GCLK_CLKCTRL_ID(GCM_TC6_TC7),   // TC7
    	  };

      // SElect Clock Gen4, set clock divisor to 8 for 6MHz clock
      GCLK->GENDIV.reg = (uint32_t) (GCLK_GENDIV_ID(GCLK_GEN4_ID) | GCLK_GENDIV_DIV(GENDIV_DIVISOR));
      while (GCLK->STATUS.bit.SYNCBUSY == 1);

      GCLK->GENCTRL.reg = (uint32_t) (GCLK_GENCTRL_ID(GCLK_GEN4_ID) | GCLK_GENCTRL_SRC_DFLL48M |
                          GCLK_GENCTRL_IDC | GCLK_GENCTRL_GENEN);
      while (GCLK->STATUS.bit.SYNCBUSY == 1);

      GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK4 | GCLK_CLKCTRL_IDs[tcNum]);
  	  while (GCLK->STATUS.bit.SYNCBUSY == 1);

  	  // Set PORT
  	  if (tcNum >= TCC_INST_NUM)
      {
    		// -- Configure TC
    		Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);
    		// Disable TCx
    		TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    		_syncTC_8(TCx);
    		// Set Timer counter Mode to 16 bits, normal PWM
    		TCx->COUNT8.CTRLA.reg |= TC_CTRLA_MODE_COUNT8 | TC_CTRLA_WAVEGEN_NPWM;
    		_syncTC_8(TCx);
        // Set PER to counter value of 240 (resolution : 0xFF)
        TCx->COUNT8.PER.reg = 0xF0;
        _syncTC_8(TCx);
    		// Set the initial value
    		TCx->COUNT8.CC[tcChannel].reg = (uint32_t) value;
    		_syncTC_8(TCx);
    		// Enable TCx
    		TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    		_syncTC_8(TCx);
  	  }
      else
      {
    		// -- Configure TCC
    		Tcc* TCCx = (Tcc*) GetTC(pinDesc.ulPWMChannel);
    		// Disable TCCx
    		TCCx->CTRLA.bit.ENABLE = 0;
    		_syncTCC(TCCx);
    		// Set TCCx as normal PWM
    		TCCx->WAVE.reg |= TCC_WAVE_WAVEGEN_NPWM;
    		_syncTCC(TCCx);
    		// Set the initial value
    		TCCx->CC[tcChannel].reg = (uint32_t) value;
    		_syncTCC(TCCx);
    		// Set PER to counter value of 240 (resolution : 0xFF)
    		TCCx->PER.reg = 0xF0;
    		_syncTCC(TCCx);
    		// Enable TCCx
    		TCCx->CTRLA.bit.ENABLE = 1;
    		_syncTCC(TCCx);
  	  }
	  }
    else
    {
  	  if (tcNum >= TCC_INST_NUM)
      {
    		Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);
    		TCx->COUNT8.CC[tcChannel].reg = (uint32_t) value;
    		_syncTC_8(TCx);
  	  }
      else
      {
    		Tcc* TCCx = (Tcc*) GetTC(pinDesc.ulPWMChannel);
    		TCCx->CTRLBSET.bit.LUPD = 1;
    		_syncTCC(TCCx);
    		TCCx->CCB[tcChannel].reg = (uint32_t) value;
    		_syncTCC(TCCx);
    		TCCx->CTRLBCLR.bit.LUPD = 1;
    		_syncTCC(TCCx);
		  }
		}
	  return;
  }

  // -- Defaults to digital write
  pinMode(pin, OUTPUT);
  if (value < 128)
  {
    digitalWrite(pin, LOW);
  }
  else
  {
    digitalWrite(pin, HIGH);
  }
}

void _syncTC_8(Tc* TCx)
{
  while (TCx->COUNT8.STATUS.bit.SYNCBUSY);
}

void _syncTCC(Tcc* TCCx)
{
  while (TCCx->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);
}

void _syncDAC()
{
  while (DAC->STATUS.bit.SYNCBUSY == 1);
}
