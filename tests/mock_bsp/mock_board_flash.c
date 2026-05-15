/* mock_board_flash.c — mock flash for host tests. */
#include "board_flash.h"
#include "bms_constants.h"
#include <string.h>
#include <stdint.h>

/* Simulate both config slots in a host buffer */
#define FLASH_CONFIG_TOTAL (CONFIG_SLOT_SIZE * 2)
static uint8_t s_flash[FLASH_CONFIG_TOTAL];

static uint32_t slot_offset(uint32_t addr) {
    if (addr >= CONFIG_A_START_ADDR && addr < CONFIG_A_START_ADDR + CONFIG_SLOT_SIZE) {
        return addr - CONFIG_A_START_ADDR;
    }
    return (addr - CONFIG_B_START_ADDR) + CONFIG_SLOT_SIZE;
}

BmsResult board_flash_erase_config_slot(uint32_t slot_addr) {
    if (slot_addr == CONFIG_A_START_ADDR) {
        memset(s_flash, 0xFF, CONFIG_SLOT_SIZE);
        return BMS_OK;
    }
    if (slot_addr == CONFIG_B_START_ADDR) {
        memset(&s_flash[CONFIG_SLOT_SIZE], 0xFF, CONFIG_SLOT_SIZE);
        return BMS_OK;
    }
    return BMS_ERR_INVALID_ARG;
}

BmsResult board_flash_write(uint32_t dst_addr, const uint8_t *buf, uint32_t len) {
    uint32_t off = slot_offset(dst_addr);
    memcpy(&s_flash[off], buf, len);
    return BMS_OK;
}

void board_flash_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    uint32_t off = slot_offset(addr);
    memcpy(buf, &s_flash[off], len);
}

/* Test helper: pre-populate a flash slot */
void mock_flash_write_slot(uint32_t slot_addr, const uint8_t *data, uint32_t len) {
    uint32_t off = slot_offset(slot_addr);
    memcpy(&s_flash[off], data, len);
}

void mock_flash_reset(void) { memset(s_flash, 0xFF, sizeof(s_flash)); }
