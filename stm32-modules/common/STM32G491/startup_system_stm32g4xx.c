/**
 ******************************************************************************
 * @file    system_stm32g4xx.c
 * @author  MCD Application Team
 * @brief   CMSIS Cortex-M4 Device Peripheral Access Layer System Source File
 *
 *   This file provides two functions and one global variable to be called from
 *   user application:
 *      - SystemInit(): This function is called at startup just after reset and
 *                      before branch to main program. This call is made inside
 *                      the "startup_stm32g4xx.s" file.
 *
 *      - SystemCoreClock variable: Contains the core clock (HCLK), it can be
 *used by the user application to setup the SysTick timer or configure other
 *parameters.
 *
 *      - SystemCoreClockUpdate(): Updates the variable SystemCoreClock and must
 *                                 be called whenever the core clock is changed
 *                                 during program execution.
 *
 */


#include "startup_system_stm32g4xx.h"

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE 24000000U /*!< Value of the External oscillator in Hz */
#endif                      /* HSE_VALUE */

#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U /*!< Value of the Internal oscillator in Hz*/
#endif                      /* HSI_VALUE */


/** @addtogroup STM32G4xx_System_Private_TypesDefinitions
 * @{
 */

/**
 * @}
 */

/** @addtogroup STM32G4xx_System_Private_Defines
 * @{
 */

/************************* Miscellaneous Configuration ************************/
/*!< Uncomment the following line if you need to relocate your vector Table in
     Internal SRAM. */
/* #define VECT_TAB_SRAM */

#define VECT_TAB_OFFSET (0) 
            /*!< Vector Table base offset field. \
              This value must be a multiple of 0x200. */
/******************************************************************************/
/**
 * @}
 */

/** @addtogroup STM32G4xx_System_Private_Macros
 * @{
 */

/**
 * @}
 */

/** @addtogroup STM32G4xx_System_Private_Variables
 * @{
 */
/* The SystemCoreClock variable is updated in three ways:
    1) by calling CMSIS function SystemCoreClockUpdate()
    2) by calling HAL API function HAL_RCC_GetHCLKFreq()
    3) each time HAL_RCC_ClockConfig() is called to configure the system clock
   frequency Note: If you use this function to configure the system clock; then
   there is no need to call the 2 first functions listed above, since
   SystemCoreClock variable is updated automatically.
*/
uint32_t SystemCoreClock = HSI_VALUE;

const uint8_t AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                   1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};


void SystemInit(void) {
    SCB->VTOR = FLASH_BASE;
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}

/**
 * @brief  Update SystemCoreClock variable according to Clock Register Values.
 *         The SystemCoreClock variable contains the core clock (HCLK), it can
 *         be used by the user application to setup the SysTick timer or
 * configure other parameters.
 *
 * @note   Each time the core clock (HCLK) changes, this function must be called
 *         to update SystemCoreClock variable value. Otherwise, any
 * configuration based on this variable will be incorrect.
 *
 * @note   - The system frequency computed by this function is not the real
 *           frequency in the chip. It is calculated based on the predefined
 *           constant and the selected clock source:
 *
 *           - If SYSCLK source is HSI, SystemCoreClock will contain the
 * HSI_VALUE(**)
 *
 *           - If SYSCLK source is HSE, SystemCoreClock will contain the
 * HSE_VALUE(***)
 *
 *           - If SYSCLK source is PLL, SystemCoreClock will contain the
 * HSE_VALUE(***) or HSI_VALUE(*) multiplied/divided by the PLL factors.
 *
 *         (**) HSI_VALUE is a constant defined in stm32g4xx_hal.h file (default
 * value 16 MHz) but the real value may vary depending on the variations in
 * voltage and temperature.
 *
 *         (***) HSE_VALUE is a constant defined in stm32g4xx_hal.h file
 * (default value 24 MHz), user has to ensure that HSE_VALUE is same as the real
 *              frequency of the crystal used. Otherwise, this function may
 *              have wrong result.
 *
 *         - The result of this function could be not correct when using
 * fractional value for HSE crystal.
 *
 * @param  None
 * @retval None
 */
void SystemCoreClockUpdate(void) {
    uint32_t tmp, pllvco, pllr, pllsource, pllm;

    /* Get SYSCLK source
     * -------------------------------------------------------*/
    switch (RCC->CFGR & RCC_CFGR_SWS) {
        case 0x04: /* HSI used as system clock source */
            SystemCoreClock = HSI_VALUE;
            break;

        case 0x08: /* HSE used as system clock source */
            SystemCoreClock = HSE_VALUE;
            break;

        case 0x0C: /* PLL used as system clock  source */
            /* PLL_VCO = (HSE_VALUE or HSI_VALUE / PLLM) * PLLN
               SYSCLK = PLL_VCO / PLLR
               */
            pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
            pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> 4) + 1U;
            if (pllsource == 0x02UL) /* HSI used as PLL clock source */
            {
                pllvco = (HSI_VALUE / pllm);
            } else /* HSE used as PLL clock source */
            {
                pllvco = (HSE_VALUE / pllm);
            }
            pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 8);
            pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> 25) + 1U) * 2U;
            SystemCoreClock = pllvco / pllr;
            break;

        default:
            break;
    }
    /* Compute HCLK clock frequency
     * --------------------------------------------*/
    /* Get HCLK prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
    /* HCLK clock frequency */
    SystemCoreClock >>= tmp;
}

void HardwareInit(void) {
    HAL_DeInit();
    HAL_Init();
    SystemClock_Config();
    SystemCoreClockUpdate();
}

// Implementatino of Error_Handler for STM32 HAL drivers
void Error_Handler(void) {
    while(1) {}
}

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/
  /* PendSV_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

  /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
  */
  HAL_PWREx_DisableUCPDDeadBattery();
}
