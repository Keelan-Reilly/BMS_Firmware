/* board_flash.c — flash config-slot erase/write for STM32F303VC. */
#include "board_flash.h"
#include "bms_hal.h"
#include <string.h>

#define FLASH_UNLOCK_KEY1  FLASH_KEYR_KEY1
#define FLASH_UNLOCK_KEY2  FLASH_KEYR_KEY2
#define FLASH_TIMEOUT_CYCLES  (100000u)

/* Validate that addr..addr+len is entirely within a config slot. */
static bool in_config_region(uint32_t addr, uint32_t len) {
    uint32_t end = addr + len;
    bool in_a = (addr >= CONFIG_A_START_ADDR) &&
                (end  <= CONFIG_A_START_ADDR + CONFIG_SLOT_SIZE);
    bool in_b = (addr >= CONFIG_B_START_ADDR) &&
                (end  <= CONFIG_B_START_ADDR + CONFIG_SLOT_SIZE);
    return in_a || in_b;
}

static BmsResult flash_unlock(void) {
    if (!(FLASH_REGS->CR & FLASH_CR_LOCK)) { return BMS_OK; }
    FLASH_REGS->KEYR = FLASH_UNLOCK_KEY1;
    FLASH_REGS->KEYR = FLASH_UNLOCK_KEY2;
    if (FLASH_REGS->CR & FLASH_CR_LOCK) { return BMS_ERR_FLASH; }
    return BMS_OK;
}

static void flash_lock(void) {
    FLASH_REGS->CR |= FLASH_CR_LOCK;
}

static BmsResult flash_wait_idle(void) {
    uint32_t t = FLASH_TIMEOUT_CYCLES;
    while ((FLASH_REGS->SR & FLASH_SR_BSY) && t) { t--; }
    return t ? BMS_OK : BMS_ERR_TIMEOUT;
}

BmsResult board_flash_erase_config_slot(uint32_t slot_addr) {
    if (slot_addr != CONFIG_A_START_ADDR && slot_addr != CONFIG_B_START_ADDR) {
        return BMS_ERR_INVALID_ARG;
    }

    BmsResult r = flash_unlock();
    if (r != BMS_OK) { return r; }

    uint32_t pages = CONFIG_SLOT_SIZE / FLASH_PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        r = flash_wait_idle();
        if (r != BMS_OK) { flash_lock(); return r; }
        FLASH_REGS->CR |= FLASH_CR_PER;
        FLASH_REGS->AR = slot_addr + p * FLASH_PAGE_SIZE;
        FLASH_REGS->CR |= FLASH_CR_STRT;
        r = flash_wait_idle();
        FLASH_REGS->CR &= ~FLASH_CR_PER;
        if (r != BMS_OK) { flash_lock(); return r; }
    }

    flash_lock();
    return BMS_OK;
}

BmsResult board_flash_write(uint32_t dst_addr, const uint8_t *buf, uint32_t len) {
    if (!in_config_region(dst_addr, len)) { return BMS_ERR_INVALID_ARG; }
    if ((dst_addr & 1u) || (len & 1u))   { return BMS_ERR_INVALID_ARG; }

    BmsResult r = flash_unlock();
    if (r != BMS_OK) { return r; }

    FLASH_REGS->CR |= FLASH_CR_PG;

    for (uint32_t i = 0; i < len; i += 2u) {
        r = flash_wait_idle();
        if (r != BMS_OK) { goto done; }

        uint16_t hw = (uint16_t)buf[i] | ((uint16_t)buf[i + 1u] << 8u);
        *((volatile uint16_t *)(dst_addr + i)) = hw;
        r = flash_wait_idle();
        if (r != BMS_OK) { goto done; }

        /* read-back verify */
        uint16_t rb = *((volatile uint16_t *)(dst_addr + i));
        if (rb != hw) { r = BMS_ERR_FLASH; goto done; }
    }

done:
    FLASH_REGS->CR &= ~FLASH_CR_PG;
    flash_lock();
    return r;
}

void board_flash_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    memcpy(buf, (const void *)addr, len);
}
