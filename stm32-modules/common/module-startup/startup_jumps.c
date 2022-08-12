#include "startup_jumps.h"

#include "startup_hal.h"

#ifndef SYSMEM_ADDRESS
#error SYSMEM_ADDRESS must be defined
#endif
#ifndef BOOTLOADER_START_ADDRESS
#error BOOTLOADER_START_ADDRESS must be defined
#endif

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const sysmem_boot_loc = (uint32_t*)BOOTLOADER_START_ADDRESS;

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const application_boot_loc = (uint32_t*)APPLICATION_START_ADDRESS;

void jump_to_bootloader(void) {

    // We have to uninitialize as many of the peripherals as possible, because the bootloader
    // expects to start as the system comes up

    // The HAL has ways to turn off all the core clocking and the clock security system
    DISABLE_CSS_FUNC();
    HAL_RCC_DeInit();

    // systick should be off at boot
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* Clear Interrupt Enable Register & Interrupt Pending Register */
    for (int i=0;i<8;i++)
    {
        NVIC->ICER[i]=0xFFFFFFFF;
        NVIC->ICPR[i]=0xFFFFFFFF;
    }

    // We have to make sure that the processor is mapping the system memory region to address 0,
    // which the bootloader expects
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    // and now we're ready to set the system up to start executing system flash.
    // arm cortex initialization means that
    // address 0 in the bootable region is the address where the processor should start its stack
    // which we have to do as late as possible because as soon as we do this the c and c++ runtime
    // environment is no longer valid
    __set_MSP(*((uint32_t*)SYSMEM_ADDRESS));

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

void jump_to_application(void) {
    asm volatile (
        "bx %0"
        : // no outputs
        : "r" (*application_boot_loc)
        : "memory"  );
}

void jump_from_exception(void) {
    // Okay, so we're in an exception (hard fault, bus fault, etc) and want to
    // jump to the DFU bootloader. We are going to jump to the function for
    // jumping to the bootloader, but to get there we have to exit this 
    // exception context. In order to do this, we have to do a few things:
    //   1. Clear the CFSR and HFSR status registers, or the bootloader will
    //      refuse to run.
    //   2. Update the PC in the exception stack frame. This step means that
    //      we HAVE to run only naked function calls, which means nothing but
    //      assembly code is allowed.
    //   3. Update the execution mode of the PSR in the exception stack frame.
    //      If this is an invalid value, the processor will be locked forever,
    //      so we force it to 0x10 for User Mode.
    //   3. Ovewrite the link register with a known exception pattern, and
    //      then return to our overwritten PC value by bx'ing to it
    asm volatile (
        /* Clear CFSR register.*/
        "ldr r0, =0xE000ED28\n"
        "ldr r1, [r0]\n"
        "str r1, [r0]\n"
        /* Clear HFSR register.*/
        "ldr r0, =0xE000ED2C\n"
        "ldr r1, [r0]\n"
        "str r1, [r0]\n"
        /* Update the PC in the stack frame.*/
        /* https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/exception-model/exception-entry-and-return */
        "ldr r0, bootloader_address_const\n"
        "str r0, [sp,#0x18]\n"
        /* In case the PSR is in invalid state, force to user mode*/
        "ldr r1, [sp,#0x1C]\n"
        "and r1, r1, #0xFFFFFFF0\n"
        "orr r1, r1, #0x10\n"
        "str r1, [sp,#0x1C]\n"
        /* Leave the exception handler */
        "ldr lr,=0xFFFFFFF1\n"
        "bx  lr\n"
        "bootloader_address_const: .word jump_to_bootloader"
        : // no outputs
        : // no inputs
        : "memory"  
    );
}
