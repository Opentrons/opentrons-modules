/**
  ******************************************************************************
  * @file   module-startup.S
  * @brief  Opentrons STM32 Modules startup application to provide a safety net
  *         in the case of device update failure.
  *         This application:
  *           - Provides an (incomplete) vector table with handlers for faults
  *             which will jump to the STM32 internal bootloader
  *           - Jumps to the second flash sector on the system in order to
  *             start the actual module firmware. If no firmware is installed,
  *             a hardfault should bring the device back to the DFU 
  *             bootloader.
  ******************************************************************************
  */

  .syntax unified
	.cpu cortex-m4
	.fpu softvfp
	.thumb

.global	g_pfnVectors
.global Fault_Handler
.global Post_Fault_Handler

.equ  BootRAM,        0xF1E0F85F

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. This simply jumps
 *          to the actual application on the second page.
 * @param  None
 * @retval : None
*/

    .section	.text.Reset_Handler
	.weak	Reset_Handler
	.type	Reset_Handler, %function
Reset_Handler:
    ldr     r0, =_estack 
    mov     sp, r0 
    /* Jump to the user application */
    ldr     r0, =_user_app_start
    ldr     r1, [r0]
    /* As an easy way to check for a missing program, check if the reset
     * vector we're expecting points to 0xFFFFFFFx. If that's the case,
     * there is definitely not a valid program to run. */
    ldr     r2, =0xFFFFFFF0
    and     r1, r1, r2
    cmp     r1, r2 
    bcs Post_Fault_Handler /* jump if upper 7 nibbles are all 0xF */
    /* Another check is to avoid any address that is not pointing to a
     * thumb-mode function, which will most certainly fault. */
    ldr     r1, [r0]
    ldr     r2, =1
    and     r1, r1, r2 
    cmp     r1, r2 
    bcc Post_Fault_Handler /* jump if bit 0 was NOT a 1 */
    /* If we didn't jump to the fault handler, jump to the app now */
    ldr     r1, [r0]
    bx      r1

.size	Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt before jumping to the main application. This
 *         sends the processor to the built in bootloader.
 *
 * @param  None
 * @retval : None
*/
    .section	.text.Fault_Handler,"ax",%progbits
    .thumb_func
Fault_Handler:
    /* First clear the Configurable Fault Status Reg (CFSR) */
    ldr     r0, =0xE000ED28 /* address of CFSR */
    ldr     r1, [r0]
    str     r1, [r0]        /* writing the reg value back clears 
                             * any asserted bits */
    /* Now clear HFSR register */
    ldr     r0, =0xE000ED2C /* address of HFSR */
    ldr     r1, [r0]
    str     r1, [r0]        /* writing the reg value back clears 
                             * any asserted bits */
    /* Now ovewrite PC in the stack frame, stored at SP + 0x18 */
    ldr     r0, =Post_Fault_Handler
    str     r0, [sp,#0x18]

    /* In order to clear the hard fault flag, we overwrite LR and return
     * to the Post_Fault_Handler function */
    bx      lr

.size	Fault_Handler, .-Fault_Handler

/**
 * @brief  This is the code that gets called by the actual hard fault handler.
 *         This sends the processor to the built in bootloader.
 *
 * @param  None
 * @retval : None
*/
    .section	.text.Post_Fault_Handler,"ax",%progbits
    .thumb_func
Post_Fault_Handler:
    /* We make the assumption that the RCC is already not initialized.*/
SystickDisable:
    /* Clear out SysTick data */
    ldr     r0, =0xE000E010UL  /* Systick base address */
    ldr     r1, =0x0           /* Reg value to write */
    str     r1, [r0]           /* CTRL */
    str     r1, [r0,0x4]       /* LOAD */
    str     r1, [r0,0x8]       /* VAL */

InitNvicClear:
    /* Clear out the NVIC enable and pending bits */
    ldr     r0, =0xE000E180UL  /* NVIC Enable base address */
    ldr     r1, =0xE000E280UL  /* NVIC Pending base address */
    ldr     r2, =0xFFFFFFFF    /* Constant */
    ldr     r3, =0             /* Loop iterator */
    b LoopNvicClear
NvicClear:
    str     r2, [r0,r3] /* Clear Enable at iterator position */
    str     r2, [r1,r3] /* Clear Pending at iterator position */
    adds r3, r3, #4     /* Increment iterator */
LoopNvicClear:
    /* 8 entries incremented by 4 = 32 */
    cmp r3, #32         /* Check end of iteration */
    bcc NvicClear       /* Branch if not done */

FlashMemorySwap:
    /* Swap mapping of flash memory for bootloader */

    /* Need to set bit 0 of the first register in SYSCFG to 1. This assumes the
     * flash is currently mapped to Main Flash Memory. */
    ldr     r0, =0x40010000  /* SYSCFG CFGR1 address. This is the same on at
                                least G491 and F303, may differ on other 
                                MCU's. */
    ldr     r1, [r0]         /* load from CFGR1 */
    orr     r1, r1, #1       /* assert first bit*/
    str     r1, [r0]         /* update value written in the register */

SetMsp:
    /* point the main stack pointer to sysmem right before jumping */
    ldr     r0, =_sysmem_start
    ldr     r1, [r0]
    msr     MSP, r1

BootloaderJump:
    /* Perform the jump to the bootloader */
    ldr     r0, =_bootloader_start
    ldr     r1, [r0]
    bx      r1

.size	Post_Fault_Handler, .-Post_Fault_Handler

/******************************************************************************
*
* The minimal vector table for a Cortex-M4.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
 	.section	.isr_vector,"a",%progbits
	.type	g_pfnVectors, %object
	.size	g_pfnVectors, .-g_pfnVectors

/* Only the core interrupts (i.e. non-positional) are defined
 * in order to maintain platform independence, and because they are
 * all unnecessary to define before jumping to the real application. */
g_pfnVectors:
	.word	_estack
	.word	Reset_Handler
	.word	Fault_Handler         /* NMI Handler */
	.word	Fault_Handler   /* HardFault Handler */
	.word	Fault_Handler   /* MemManage Handler */
	.word	Fault_Handler    /* BusFault_Handler */
	.word	Fault_Handler  /* UsageFault_Handler */
	.word	0
	.word	0
	.word	0
	.word	0
	.word	Fault_Handler         /* SVC_Handler */
	.word	Fault_Handler    /* DebugMon_Handler */
	.word	0
	.word	Fault_Handler      /* DebugMon_Handler */
	.word	Fault_Handler     /* SysTick_Handler */
