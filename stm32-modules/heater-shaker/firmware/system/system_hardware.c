#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_it.h"
#include "stm32f3xx_hal_cortex.h"
#include "system_hardware.h"
#include "systemwide.h"
#include "FreeRTOS.h" //need?
#include "task.h"
#include <string.h>

system_hardware_handles *SYSTEM_HW_HANDLE = NULL;
static TaskHandle_t xTaskToNotify = NULL;

/* I2C handler declaration */
I2C_HandleTypeDef I2cHandle;

uint8_t PWMInitBuffer[SYSTEM_WIDE_TXBUFFERSIZE] = {LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI};
uint8_t OutputInitBuffer[SYSTEM_WIDE_TXBUFFERSIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t UpdateBuffer[1] = {0};
uint8_t ShutdownBuffer[1] = {1};

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

    if (!system_hardware_setup_led()) {
      //throw error?!
    }
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

//bool system_hardware_set_led(uint8_t aTxBuffer[SYSTEM_WIDE_TXBUFFERSIZE], I2C_Operations operation) {
bool system_hardware_set_led_original(uint8_t* aTxBuffer, I2C_Operations operation) {
  //loop thru bits
  //if bit_previous != bit
    //if 1, set register high
    //else set low
  //enum for bit-to-register table? or just add bit # to base_register?
  //store current state as previous state

  //ensure state initialized as all low and stored

  //use address auto increment
  //loop thru aTxBuffer, use SYSTEM_WIDE_TXBUFFERSIZE

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

  uint16_t base_register = 0;

  switch(operation) {
    case PWM_Init:
      base_register = (uint16_t)BASE_PWM_REGISTER;
      break;
    case LED_Control:
      base_register = (uint16_t)BASE_REGISTER;
      break;
  }

  //copy array into file global array
  //write, take, write, take, give in ISR (give error if error callback). ulTaskNotifyTake

  HAL_StatusTypeDef status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, base_register,
    (uint16_t)REGISTER_SIZE, (uint8_t*)aTxBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
  //if ((status == HAL_OK) && (operation == LED_Control)) { //only update after LED control transmit at initialization
  //  status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)UPDATE_REGISTER,
  //    (uint16_t)REGISTER_SIZE, (uint8_t*)UpdateBuffer, (uint16_t)(sizeof(UpdateBuffer)));
  //}
  return (status == HAL_OK); //translate into error/no error at policy level, create Ack Message at end of StartSetLED System_Task function
}

//debug function. Confirm all HAL function inputs correct
bool system_hardware_set_led_test(uint8_t step, uint8_t which) {
  HAL_StatusTypeDef status;
  switch(step) {
    case 0:
      status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)0x00,
        (uint16_t)REGISTER_SIZE, (uint8_t*)ShutdownBuffer, (uint16_t)(sizeof(ShutdownBuffer)));
      break;
    case 1:
      status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)BASE_PWM_REGISTER,
        (uint16_t)REGISTER_SIZE, (uint8_t*)PWMInitBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
      break;
    case 2:
      OutputInitBuffer[which] = (OutputInitBuffer[which] == 0x00) ? LED_OUT_HI : 0x00;
      status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)BASE_REGISTER,
        (uint16_t)REGISTER_SIZE, (uint8_t*)OutputInitBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
      break;
    case 3:
      status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)UPDATE_REGISTER,
        (uint16_t)REGISTER_SIZE, (uint8_t*)UpdateBuffer, (uint16_t)(sizeof(UpdateBuffer)));
      break;
    default:
      status = HAL_ERROR;
  }
  return (status == HAL_OK);
}

//proper error handling. Enumarate more, detailed errors
//does NotifyTake block? Just system task?

bool system_hardware_setup_led(void) {
  uint32_t ulNotificationValue;
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );
  HAL_StatusTypeDef status;

  configASSERT( xTaskToNotify == NULL );
  xTaskToNotify = xTaskGetCurrentTaskHandle();
  status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)SHUTDOWN_REGISTER,
    (uint16_t)REGISTER_SIZE, (uint8_t*)ShutdownBuffer, (uint16_t)(sizeof(ShutdownBuffer)));
  ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

  if ((status == HAL_OK) && (ulNotificationValue == 1)) {
    configASSERT( xTaskToNotify == NULL );
    xTaskToNotify = xTaskGetCurrentTaskHandle();
    status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)BASE_PWM_REGISTER,
      (uint16_t)REGISTER_SIZE, (uint8_t*)PWMInitBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
    ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

    if ((status == HAL_OK) && (ulNotificationValue == 1)) {
      configASSERT( xTaskToNotify == NULL );
      xTaskToNotify = xTaskGetCurrentTaskHandle();
      status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)BASE_REGISTER,
        (uint16_t)REGISTER_SIZE, (uint8_t*)OutputInitBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
      ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

      if ((status == HAL_OK) && (ulNotificationValue == 1)) {
        configASSERT( xTaskToNotify == NULL );
        xTaskToNotify = xTaskGetCurrentTaskHandle();
        status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)UPDATE_REGISTER,
          (uint16_t)REGISTER_SIZE, (uint8_t*)UpdateBuffer, (uint16_t)(sizeof(UpdateBuffer)));
        ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

      }
    }
  }

  return ((status == HAL_OK) && (ulNotificationValue == 1));
}

bool system_hardware_set_led(uint8_t* aTxBuffer) {
  uint32_t ulNotificationValue;
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );
  HAL_StatusTypeDef status;

  memcpy(LEDOutputBuffer, aTxBuffer, sizeof(LEDOutputBuffer));

  configASSERT( xTaskToNotify == NULL );
  xTaskToNotify = xTaskGetCurrentTaskHandle();
  status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)BASE_REGISTER,
    (uint16_t)REGISTER_SIZE, (uint8_t*)LEDOutputBuffer, (uint16_t)SYSTEM_WIDE_TXBUFFERSIZE);
  ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

  if ((status == HAL_OK) && (ulNotificationValue == 1)) {
    configASSERT( xTaskToNotify == NULL );
    xTaskToNotify = xTaskGetCurrentTaskHandle();
    status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)UPDATE_REGISTER,
      (uint16_t)REGISTER_SIZE, (uint8_t*)UpdateBuffer, (uint16_t)(sizeof(UpdateBuffer)));
    ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

  }

  return ((status == HAL_OK) && (ulNotificationValue == 1));
}

bool system_hardware_I2C_ready(void) {
  return (HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_READY);
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  configASSERT( xTaskToNotify != NULL );
  if (I2cHandle->State == HAL_I2C_STATE_READY) { //right way to check this error?
    vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
  }
  xTaskToNotify = NULL;
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

  //vTaskNotifyGiveFromISR(receiver_handle, &xHigherPriorityTaskWoken);
  //signal that the transfer has completed successfully
  /*led_transmit_result result;
  if (I2cHandle->State == HAL_I2C_STATE_READY) {
    result.success = true;
  } else {
    result.success = false;
  }
  SYSTEM_HW_HANDLE->led_transmit_complete(&result);*/
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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  configASSERT( xTaskToNotify != NULL );
  if (!HAL_I2C_GetError(I2cHandle)) { //there's implicitly an error, so even necessary? Just don't give notify back?
    vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
  }
  xTaskToNotify = NULL;
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

  /*led_transmit_result result;
  result.success = false; //need?
  if (HAL_I2C_GetError(I2cHandle) != HAL_I2C_ERROR_AF) { //does this signal error?!
    result.error = true; //can we get specific error?
  }
  SYSTEM_HW_HANDLE->led_transmit_error_complete(&result);*/
}

//*********** was in example project, but think including is wrong and cause of build errors *************
/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
/*void SysTick_Handler(void)
{
  HAL_IncTick();
}*/

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
