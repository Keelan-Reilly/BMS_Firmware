/* bootloader/src/main.c — STM32F303VC bootloader entry point. SKELETON.
 *
 * Boot decision (from docs/06_flash_and_bootloader.md):
 *   1. Check RTC BKP0R for boot flag → stay in bootloader if set.
 *   2. Validate application SP and reset vector.
 *   3. Verify application CRC (STUB — not yet implemented).
 *   4. Jump to application.
 */
#include "bl_config.h"
#include "bl_validate.h"
#include "bl_jump.h"
#include <stdint.h>
#include <stdbool.h>

/* Minimal clock init for bootloader: use HSI 8MHz (no PLL needed for UART) */
static void bl_clock_init(void) {
    /* HSI already on at reset — no action needed for bootloader UART at 115200 */
}

/* Minimal GPIO/UART init: STUB — implement for bootloader protocol */
static void bl_uart_init(void) {
    /* STUB */
}

/* RTC boot flag check */
static bool bl_check_boot_flag(void) {
    volatile uint32_t *bkp0r = (volatile uint32_t *)0x40002850u; /* RTC BKP0R */
    if (*bkp0r == BL_ENTRY_FLAG) {
        *bkp0r = 0u; /* clear flag */
        return true;
    }
    return false;
}

/* Read DBGMCU DEV_ID */
static uint32_t bl_read_mcu_dev_id(void) {
    volatile uint32_t *idcode = (volatile uint32_t *)DBGMCU_IDCODE_ADDR;
    return *idcode & DBGMCU_DEV_ID_MASK;
}

static void bl_protocol_run(void) {
    /* STUB: respond to GET_CAPABILITIES and bootloader update packets over UART */
    while (1) { /* spin until protocol implemented */ }
}

int main(void) {
    bl_clock_init();

    /* Check boot flag first */
    if (bl_check_boot_flag()) {
        bl_uart_init();
        bl_protocol_run();
    }

    /* Validate application image */
    volatile uint32_t *vtable = (volatile uint32_t *)APP_START_ADDR;
    uint32_t app_sp = vtable[0];
    uint32_t app_rv = vtable[1];

    if (!bl_is_valid_sp(app_sp) || !bl_is_valid_reset_vector(app_rv)) {
        bl_uart_init();
        bl_protocol_run();
    }

    /* STUB: CRC verification of app image — not yet implemented */
    /* When implemented: compute CRC over [APP_START_ADDR .. APP_START_ADDR + app_size]
     * and compare against stored value. On failure: bl_protocol_run(). */

    /* Jump to application */
    bl_jump_to_app(APP_START_ADDR);
}
