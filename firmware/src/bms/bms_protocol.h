/* bms_protocol.h — UART protocol framing and packet dispatch. */
#pragma once
#include <stdint.h>
#include "bms_types.h"

/* Initialise protocol state machine. */
void bms_protocol_init(void);

/* Process any bytes available in the UART RX buffer.
 * When a complete frame is received, dispatches to the appropriate handler
 * and sends a response. Call every main loop iteration. */
void bms_protocol_tick(void);

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no final XOR).
 * Covers all bytes from SOF through payload. */
uint16_t bms_protocol_crc16(const uint8_t *data, uint16_t len);
