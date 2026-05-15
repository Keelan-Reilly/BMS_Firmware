/* board_spi.h — SPI1 driver for the isoSPI bus.
 *
 * SPI1 is shared between CELL and TEMP chains via separate CS lines.
 * Only one CS may be asserted at a time.
 * Mode 3 (CPOL=1, CPHA=1), MSB first, 8-bit frames.
 * CS lines are software-controlled GPIO (active-low, idle HIGH).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "bms_types.h"

/* Init SPI1 GPIO and peripheral. CS lines initialised to HIGH (idle). */
void board_spi_init(void);

/* Assert/deassert a chain's CS. Never call for both chains simultaneously. */
void board_spi_cs_assert(BmsChain chain);
void board_spi_cs_deassert(BmsChain chain);

/* Transfer len bytes (full-duplex). tx or rx may be NULL for write-only /
 * read-only transfers. CS must already be asserted before calling. */
void board_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

/* Convenience: send only (rx discarded). */
void board_spi_write(const uint8_t *tx, uint16_t len);

/* Returns true if a CS is currently asserted (for debug assertion checks). */
bool board_spi_is_busy(void);
