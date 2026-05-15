/* mock_board_spi.c — mock SPI implementation for host tests. */
#include "mock_board_spi.h"
#include "board_spi.h"
#include <string.h>

#define MOCK_BUF_SIZE 512

static uint8_t  s_rx_buf[MOCK_BUF_SIZE];
static uint16_t s_rx_len;
static uint16_t s_rx_pos;

static uint8_t  s_tx_buf[MOCK_BUF_SIZE];
static uint16_t s_tx_len;

static bool s_busy;

void mock_spi_set_rx(const uint8_t *data, uint16_t len) {
    uint16_t copy = len < MOCK_BUF_SIZE ? len : MOCK_BUF_SIZE;
    memcpy(s_rx_buf, data, copy);
    s_rx_len = copy;
    s_rx_pos = 0;
}

const uint8_t *mock_spi_get_last_tx(void)     { return s_tx_buf; }
uint16_t       mock_spi_get_last_tx_len(void)  { return s_tx_len; }

void mock_spi_reset(void) {
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    memset(s_tx_buf, 0, sizeof(s_tx_buf));
    s_rx_len = s_rx_pos = s_tx_len = 0;
    s_busy = false;
}

/* ── board_spi.h implementation ──────────────────────────────────────────── */
void board_spi_init(void) { mock_spi_reset(); }

void board_spi_cs_assert(BmsChain chain)   { (void)chain; s_busy = true;  }
void board_spi_cs_deassert(BmsChain chain) { (void)chain; s_busy = false; }

void board_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len) {
    if (tx && s_tx_len + len < MOCK_BUF_SIZE) {
        memcpy(&s_tx_buf[s_tx_len], tx, len);
        s_tx_len += len;
    }
    if (rx) {
        for (uint16_t i = 0; i < len; i++) {
            rx[i] = (s_rx_pos < s_rx_len) ? s_rx_buf[s_rx_pos++] : 0xFFu;
        }
    }
}

void board_spi_write(const uint8_t *tx, uint16_t len) {
    board_spi_transfer(tx, NULL, len);
}

bool board_spi_is_busy(void) { return s_busy; }
