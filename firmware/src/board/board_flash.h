/* board_flash.h — flash erase/write for config storage.
 * Does NOT expose a general-purpose flash API; only config-region operations.
 * Safety: erase and write are refused outside the config slot addresses.
 */
#pragma once
#include <stdint.h>
#include "bms_types.h"
#include "bms_constants.h"

/* Erase one config slot (one 8 KB region = 4 × 2 KB pages).
 * slot_addr must be CONFIG_A_START_ADDR or CONFIG_B_START_ADDR. */
BmsResult board_flash_erase_config_slot(uint32_t slot_addr);

/* Write len bytes from buf to flash starting at dst_addr.
 * dst_addr and len must be within a config slot and half-word aligned.
 * Performs read-back verification after each half-word write. */
BmsResult board_flash_write(uint32_t dst_addr, const uint8_t *buf, uint32_t len);

/* Read len bytes from flash addr into buf (simple memory copy via pointer). */
void board_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
