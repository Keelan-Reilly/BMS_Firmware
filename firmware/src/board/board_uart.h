/* board_uart.h — USART2 driver for the BMS protocol UART link. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Init USART2 at 115200 8N1 on PA2(TX)/PA3(RX). */
void board_uart_init(void);

/* Transmit exactly len bytes; blocks until all bytes sent (polling). */
void board_uart_write(const uint8_t *data, uint16_t len);

/* Returns true if a byte is available in the RX shift register. */
bool board_uart_rx_ready(void);

/* Read one byte; only call when board_uart_rx_ready() is true. */
uint8_t board_uart_read_byte(void);

/* Non-blocking read into buf; returns bytes actually read. */
uint16_t board_uart_read(uint8_t *buf, uint16_t max_len);
