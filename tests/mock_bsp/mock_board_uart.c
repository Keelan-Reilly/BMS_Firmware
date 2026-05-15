/* mock_board_uart.c — mock UART for host tests. */
#include "board_uart.h"
#include <string.h>
#define MOCK_TX_SIZE 2048
static uint8_t s_tx[MOCK_TX_SIZE];
static uint16_t s_tx_len;
void board_uart_init(void) { s_tx_len = 0; }
void board_uart_write(const uint8_t *data, uint16_t len) {
    if (s_tx_len + len < MOCK_TX_SIZE) {
        memcpy(&s_tx[s_tx_len], data, len);
        s_tx_len += len;
    }
}
bool board_uart_rx_ready(void) { return false; }
uint8_t board_uart_read_byte(void) { return 0; }
uint16_t board_uart_read(uint8_t *buf, uint16_t max_len) { (void)buf; (void)max_len; return 0; }
const uint8_t *mock_uart_get_tx(void) { return s_tx; }
uint16_t mock_uart_get_tx_len(void) { return s_tx_len; }
void mock_uart_reset(void) { s_tx_len = 0; }
