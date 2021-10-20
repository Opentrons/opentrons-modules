#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_it.h"
#include "stm32f3xx_hal_cortex.h"
#include "system_hardware.h"

system_hardware_handles *SYSTEM_HW_HANDLE = NULL;

/* I2C handler declaration */
I2C_HandleTypeDef I2cHandle;

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

    /*##-1- Configure the I2C peripheral ######################################*/
    I2cHandle.Instance             = I2Cx;
    I2cHandle.Init.Timing          = I2C_TIMING;
    I2cHandle.Init.OwnAddress1     = I2C_ADDRESS;
    I2cHandle.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2cHandle.Init.OwnAddress2     = 0xFF;
    I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&I2cHandle);
    HAL_I2CEx_ConfigAnalogFilter(&I2cHandle,I2C_ANALOGFILTER_ENABLE); //do this?

    uint8_t PWMInitBuffer[TXBUFFERSIZE] = {LED_PWM_OUTPUT_HIGH};
    system_hardware_set_led(PWMInitBuffer, PWM_Init);
    uint8_t OutputInitBuffer[TXBUFFERSIZE] = {0}; //make all zeroes
    system_hardware_set_led(OutputInitBuffer, LED_Control);
    //any error checking?
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

/**
  * @brief I2C MSP Initialization 
  *        This function configures the hardware resources used in this example: 
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration  
  *           - DMA configuration for transmission request by peripheral 
  *           - NVIC configuration for DMA interrupt request enable
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef  RCC_PeriphCLKInitStruct;
  
  /*##-1- Configure the I2C clock source. The clock is derived from the SYSCLK #*/
  RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2Cx;
  RCC_PeriphCLKInitStruct.I2c1ClockSelection = RCC_I2CxCLKSOURCE_SYSCLK;
  HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);

  /*##-2- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  I2Cx_SCL_GPIO_CLK_ENABLE();
  I2Cx_SDA_GPIO_CLK_ENABLE();
  /* Enable I2Cx clock */
  I2Cx_CLK_ENABLE(); 

  /*##-3- Configure peripheral GPIO ##########################################*/  
  /* I2C TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = I2Cx_SCL_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = I2Cx_SCL_SDA_AF;
  HAL_GPIO_Init(I2Cx_SCL_GPIO_PORT, &GPIO_InitStruct);
    
  /* I2C RX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = I2Cx_SDA_PIN;
  GPIO_InitStruct.Alternate = I2Cx_SCL_SDA_AF;
  HAL_GPIO_Init(I2Cx_SDA_GPIO_PORT, &GPIO_InitStruct);
    
  /*##-4- Configure the NVIC for I2C ########################################*/   
  /* NVIC for I2Cx */
  HAL_NVIC_SetPriority(I2Cx_ER_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(I2Cx_ER_IRQn);
  HAL_NVIC_SetPriority(I2Cx_EV_IRQn, 0, 2);
  HAL_NVIC_EnableIRQ(I2Cx_EV_IRQn);
}

/**
  * @brief I2C MSP De-Initialization 
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO, DMA and NVIC configuration to their default state
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
  
  /*##-1- Reset peripherals ##################################################*/
  I2Cx_FORCE_RESET();
  I2Cx_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure I2C Tx as alternate function  */
  HAL_GPIO_DeInit(I2Cx_SCL_GPIO_PORT, I2Cx_SCL_PIN);
  /* Configure I2C Rx as alternate function  */
  HAL_GPIO_DeInit(I2Cx_SDA_GPIO_PORT, I2Cx_SDA_PIN);
  
  /*##-3- Disable the NVIC for I2C ##########################################*/
  HAL_NVIC_DisableIRQ(I2Cx_ER_IRQn);
  HAL_NVIC_DisableIRQ(I2Cx_EV_IRQn);
}

bool system_hardware_set_led(uint8_t aTxBuffer[TXBUFFERSIZE], I2C_Operations operation) {
  //loop thru bits
  //if bit_previous != bit
    //if 1, set register high
    //else set low
  //enum for bit-to-register table? or just add bit # to base_register?
  //store current state as previous state

  //ensure state initialized as all low and stored

  //use address auto increment
  //loop thru aTxBuffer, use TXBUFFERSIZE

  //make TXBUFFERSIZE part of systemwide? Get rid of #define in this header
  //how to include systemwide.hpp in .h file??
  
  //eventually have a enum table at high level for mapping LEDs (and their states) to array index/state
  //#define LED_OUTPUT_HIGH
  //have system UpdateLED visit_message first check if I2C ready (policy->system_hardware_I2C_ready),
    //if not ready, send another UpdateLED message?!

  //check all files in stm32tools template project

  //do transmit like main.c lines 132-154? How does that syntax work?
  //Should error check. Use HAL_I2C_ErrorCallback()?!
  //won't callback signal end of transmission?
  //returns bool for transmit_start_success

  uint16_t base_register;
  uint8_t UpdateBuffer[1] = {};

  switch(operation) {
    case PWM_Init:
      base_register = (uint16_t)BASE_PWM_REGISTER;
      break;
    case LED_Control:
      base_register = (uint16_t)BASE_REGISTER;
      break;
  }

  HAL_StatusTypeDef status = HAL_I2C_Mem_Write_IT(&I2cHandle, (uint16_t)I2C_ADDRESS, base_register,
    (uint16_t)REGISTER_SIZE, (uint8_t*)aTxBuffer, TXBUFFERSIZE);
  if ((status == HAL_OK) && (operation == LED_Control) { //only update after LED control transmit at initialization
    status = HAL_I2C_Mem_Write_IT(&I2cHandle, (uint16_t)I2C_ADDRESS, (uint16_t)UPDATE_REGISTER,
      (uint16_t)REGISTER_SIZE, (uint8_t*)UpdateBuffer, SIZEOF(UpdateBuffer));
  }
  return (status == HAL_OK);

/*HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                       uint16_t MemAddSize, uint8_t *pData, uint16_t Size)

HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData,
                                             uint16_t Size)*/
}

bool system_hardware_I2C_ready(void) {
  return (HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_READY);
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

/**
  * @brief  I2C error callbacks.
  * @param  I2cHandle: I2C handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *I2cHandle)
{
  /** 1- When Slave don't acknowledge it's address, Master restarts communication.
    * 2- When Master don't acknowledge the last data transferred, Slave don't care in this example.
    */
  led_transmit_result result;
  result.success = false; //need?
  if (HAL_I2C_GetError(I2cHandle) != HAL_I2C_ERROR_AF) { //does this signal error?!
    result.error = true; //can we get specific error?
  }
  SYSTEM_HW_HANDLE->led_transmit_error_complete(&result);
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/*                 STM32F3xx Peripherals Interrupt Handlers                  */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f3xx.s).                                               */
/******************************************************************************/
/**
  * @brief  This function handles I2C event interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to I2C data transmission
  */
void I2Cx_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&I2cHandle);
}

/**
  * @brief  This function handles I2C error interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to I2C error
  */
void I2Cx_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&I2cHandle);
}
