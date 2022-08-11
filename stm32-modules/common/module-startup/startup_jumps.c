#include "startup_jumps.h"

#include "startup_hal.h"

void jump_to_bootloader(void) {
    asm volatile (
        ""
        : // no outputs
        : // no inputs
        : "memory"  
    );
}

void jump_to_application(void) {

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
        "bootloader_address_const: .word system_hardware_enter_bootloader"
        : // no outputs
        : // no inputs
        : "memory"  
    );
}
