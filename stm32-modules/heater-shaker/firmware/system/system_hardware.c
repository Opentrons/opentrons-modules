#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_cortex.h"
#include "system_hardware.h"

system_hardware_handles *SYSTEM_HW_HANDLE = NULL;

void system_hardware_setup(system_hardware_handles* handles) {
    SYSTEM_HW_HANDLE = handles;
    GPIO_InitTypeDef gpio_init = {
      .Pin = SOFTPOWER_BUTTON_SENSE_PIN | SOFTPOWER_UNPLUG_SENSE_PIN,
      .Mode = GPIO_MODE_INPUT,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW,
    };
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_Init(SOFTPOWER_PORT, &gpio_init);
}

// This is the start of the sys memory region from the datasheet. It should be the
// same for at least all the f303s, including the vdt6 which is on the nffs and the
// vc that will be on the ff.
#define SYSMEM_START 0x1fffd800
#define SYSMEM_BOOT (SYSMEM_START + 4)

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const sysmem_boot_loc = (uint32_t*)SYSMEM_BOOT;

void system_hardware_enter_bootloader(void) {

  // We have to uninitialize as many of the peripherals as possible, because the bootloader
  // expects to start as the system comes up

  // The HAL has ways to turn off all the core clocking and the clock security system
  HAL_RCC_DisableCSS();
  HAL_RCC_DeInit();

  // systick should be off at boot
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // We have to make sure that the processor is mapping the system memory region to address 0,
  // which the bootloader expects
  __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
  // and now we're ready to set the system up to start executing system flash.
  // arm cortex initialization means that

  // address 0 in the bootable region is the address where the processor should start its stack
  // which we have to do as late as possible because as soon as we do this the c and c++ runtime
  // environment is no longer valid
  __set_MSP(*((uint32_t*)SYSMEM_START));

  // finally, jump to the bootloader. we do this in inline asm because we need
  // this to be a naked call (no caller-side prep like stacking return addresses)
  // and to have a naked function you need to define it as a function, not a
  // function pointer, and we don't statically know the address here since it is
  // whatever's contained in that second word of the bsystem memory region.
asm volatile (
  "bx %0"
  : // no outputs
  : "r" (*sysmem_boot_loc)
  : "memory"  );
}

/* I2C handler declaration */
I2C_HandleTypeDef I2cHandle;

/* Buffer used for transmission */
uint8_t aTxBuffer[] = " ****I2C_TwoBoards communication based on IT****  ****I2C_TwoBoards communication based on IT****  ****I2C_TwoBoards communication based on IT**** ";

/* Buffer used for reception */
uint8_t aRxBuffer[RXBUFFERSIZE];

/*##-1- Configure the I2C peripheral ######################################*/
I2cHandle.Instance             = I2Cx;
I2cHandle.Init.Timing          = I2C_TIMING;
I2cHandle.Init.OwnAddress1     = I2C_ADDRESS;
I2cHandle.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
I2cHandle.Init.OwnAddress2     = 0xFF;
I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

void system_hardware_led_change(uint16_t register_map) {
  //loop thru bits
  //if bit_previous != bit
    //if 1, set register high
    //else set low
  //enum for bit-to-register table? or just add bit # to base_register?
  //store current state as previous state

  //ensure state initialized as all low and stored

}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
  //signal that the transfer has completed successfully
  led_transmit_result result;
  if (I2cHandle->State == HAL_I2C_STATE_READY) {
    result.success = true;
  } else {
    result.success = false;
  }
  SYSTEM_HW_HANDLE->led_transmit_complete(&result);
}
