#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_cortex.h"
#include "system_hardware.h"

void system_hardware_setup(void) {
    GPIO_InitTypeDef gpio_init = {
      .Pin = SOFTPOWER_BUTTON_SENSE_PIN | SOFTPOWER_UNPLUG_SENSE_PIN,
      .Mode = GPIO_MODE_INPUT,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW,
    };
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_Init(SOFTPOWER_PORT, &gpio_init);
}

void system_hardware_enter_bootloader(void) {
  // a magic function pointer that we'll bind to the first code address in system
  // memory
  void (*sysmem_boot)(void);
  // This is the start of the sys memory region from the datasheet. It should be the
  // same for at least all the f303s, including the vdt6 which is on the nffs and the
  // vc that will be on the ff.
  volatile uint32_t sysmem_start = 0x1FFFD800;
  // We have to uninitialize as many of the peripherals as possible, because the bootloader
  // expects to start as the system comes up, which means we better not have systick running
  // (SysTick is a magic global defined by cmsis)
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  // we should get our interrupts disabled (__disable_irq is a magic intrinsic defined by CMSIS)
  __disable_irq();
  // The HAL has ways to turn off all the peripheral clocks and the clock security system
  HAL_RCC_DisableCSS();
  HAL_RCC_DeInit();
  // We have to make sure that the processor is mapping the system memory region to address 0,
  // which the bootloader expects
  __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
  // and now we're ready to set the system up to start executing system flash.
  // arm cortex initialization means that
  // - address 4 in the bootable region is the address of the first instruction that
  //   should run, aka the data that should be loaded into $pc, so we want to make our
  //   function pointer be
  sysmem_boot = (void (*)(void))( // a cast to something that looks like a pointer-to-function
                                  // that takes no arguments and returns no values
    *(                            // of the value, which should itself be a memory access,
      (uint32_t*)(sysmem_start + 4))); // 4 bytes past the magic value we stored above

  // - address 0 in the bootable region is the address where the processor should start its stack
  //   which we have to do as late as possible because as soon as we do this the c and c++ runtime
  //   environment is no longer valid
  __set_MSP(*((uint32_t*)sysmem_start));
  sysmem_boot();
}
