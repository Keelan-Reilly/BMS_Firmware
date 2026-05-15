/* bms_hal.h — hardware abstraction shim for the BMS board layer.
 *
 * Pulls in the official STM32F3 CMSIS device header (stm32f3xx.h → stm32f303xc.h
 * → core_cm4.h) and provides the small set of convenience aliases our board code
 * relies on that differ from or are absent in the official names.
 *
 * Nothing in this file changes register layouts or addresses.
 * Do not add peripheral initialisations here.
 */
#pragma once

/* ── Official CMSIS / ST device header ───────────────────────────────────── */
/* Requires STM32F303xC defined at compile time (-DSTM32F303xC). */
#include "stm32f3xx.h"

/* ── Flash register alias ─────────────────────────────────────────────────── */
/* Official header names the Flash controller peripheral macro FLASH;
 * our board_flash.c and board_clock.c use FLASH_REGS. */
#define FLASH_REGS      FLASH

/* Official Flash unlock keys are FLASH_KEY1 / FLASH_KEY2.
 * board_flash.c was written with FLASH_KEYR_KEY1 / FLASH_KEYR_KEY2. */
#define FLASH_KEYR_KEY1 FLASH_KEY1
#define FLASH_KEYR_KEY2 FLASH_KEY2

/* ── GPIO MODER field values ──────────────────────────────────────────────── */
/* The official device header does not define single-field symbolic constants
 * for the 2-bit MODER field; we define them here. */
#define GPIO_MODER_INPUT   0x0u
#define GPIO_MODER_OUTPUT  0x1u
#define GPIO_MODER_AF      0x2u
#define GPIO_MODER_ANALOG  0x3u

/* ── SysTick convenience aliases ─────────────────────────────────────────── */
/* CMSIS core_cm4.h uses _Msk suffix; board_clock.c omits it. */
#define SysTick_CTRL_ENABLE    SysTick_CTRL_ENABLE_Msk
#define SysTick_CTRL_TICKINT   SysTick_CTRL_TICKINT_Msk
#define SysTick_CTRL_CLKSOURCE SysTick_CTRL_CLKSOURCE_Msk

/* ── SCB AIRCR aliases ────────────────────────────────────────────────────── */
/* CMSIS uses _Msk suffix; bms_state.c uses bare name. */
#define SCB_AIRCR_SYSRESETREQ  SCB_AIRCR_SYSRESETREQ_Msk
/* VECTKEY write value: upper 16 bits must be 0x05FA to unlock AIRCR writes. */
#define SCB_AIRCR_VECTKEY      (0x05FAuL << 16u)

/* ── IWDG key values ─────────────────────────────────────────────────────── */
/* Official header only defines IWDG_KR_KEY (the field mask); the magic write
 * values for reload/unlock/enable are not named there. */
#define IWDG_KR_RELOAD  0xAAAAu
#define IWDG_KR_UNLOCK  0x5555u
#define IWDG_KR_ENABLE  0xCCCCu
