/* mock_board_spi.h — mock SPI driver for host-compiled tests. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "bms_types.h"

/* Configure mock SPI to return specific RX bytes on next transfer */
void mock_spi_set_rx(const uint8_t *data, uint16_t len);

/* Return last bytes transmitted by the firmware code */
const uint8_t *mock_spi_get_last_tx(void);
uint16_t       mock_spi_get_last_tx_len(void);

/* Reset mock state */
void mock_spi_reset(void);
