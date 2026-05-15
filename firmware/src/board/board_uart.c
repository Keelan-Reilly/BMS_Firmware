/* board_uart.c — USART2 init and polling IO (PA2 TX, PA3 RX). */
#include "board_uart.h"
#include "board_pins.h"
#include "bms_hal.h"
#include "bms_constants.h"

void board_uart_init(void) {
    /* PA2 TX, PA3 RX → AF7 */
    /* MODER: pins 2,3 → AF (0x2) */
    UART_PORT->MODER &= ~(0xFu << 4);
    UART_PORT->MODER |= (GPIO_MODER_AF << 4) | (GPIO_MODER_AF << 6);
    /* AFR[0] bits [11:8]=AF7 (pin2), [15:12]=AF7 (pin3) */
    UART_PORT->AFR[0] &= ~(0xFFu << 8);
    UART_PORT->AFR[0] |= (UART_AF << 8) | (UART_AF << 12);

    /* USART2: 115200 baud at PCLK1 = 36 MHz → BRR = 36e6 / 115200 ≈ 313 */
    USART2->BRR = (uint32_t)(36000000u / UART_BAUD_RATE);
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void board_uart_write(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        while (!(USART2->ISR & USART_ISR_TXE)) { /* wait for TX empty */ }
        USART2->TDR = data[i];
    }
    /* wait for last byte to shift out */
    while (!(USART2->ISR & USART_ISR_TC)) { /* spin */ }
}

bool board_uart_rx_ready(void) {
    return (USART2->ISR & USART_ISR_RXNE) != 0u;
}

uint8_t board_uart_read_byte(void) {
    return (uint8_t)(USART2->RDR & 0xFFu);
}

uint16_t board_uart_read(uint8_t *buf, uint16_t max_len) {
    uint16_t n = 0;
    while (n < max_len && board_uart_rx_ready()) {
        buf[n++] = board_uart_read_byte();
    }
    return n;
}
