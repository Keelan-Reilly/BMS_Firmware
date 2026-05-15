/* bl_jump.c — safe jump to application. */
#include "bl_jump.h"
#include "bl_config.h"

bool bl_is_valid_sp(uint32_t sp) {
    /* SP must be within SRAM: 0x20000000 to 0x20009FFF (40KB) */
    return (sp >= 0x20000000u) && (sp <= 0x2000A000u) && ((sp & 3u) == 0u);
}

bool bl_is_valid_reset_vector(uint32_t rv) {
    /* Reset vector must be an odd Thumb address within the app region */
    return (rv >= APP_START_ADDR + 1u) &&
           (rv < APP_START_ADDR + APP_REGION_SIZE) &&
           (rv & 1u);   /* Thumb bit must be set */
}

void bl_jump_to_app(uint32_t app_addr) {
#ifndef BMS_HOST_BUILD
    /* Disable all IRQs */
    __asm volatile ("cpsid i" ::: "memory");

    /* Set VTOR to application vector table */
    volatile uint32_t *vtor = (volatile uint32_t *)0xE000ED08u;
    *vtor = app_addr;

    /* Load application SP and reset vector */
    volatile uint32_t *vtable = (volatile uint32_t *)app_addr;
    uint32_t app_sp  = vtable[0];
    uint32_t app_rv  = vtable[1];

    /* Trampoline: set SP and branch to reset vector */
    __asm volatile (
        "msr msp, %0\n"
        "bx  %1\n"
        :
        : "r" (app_sp), "r" (app_rv)
        :
    );
#else
    (void)app_addr; /* never called in host test build */
#endif
    while (1) {}
}
