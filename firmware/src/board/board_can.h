/* board_can.h — bxCAN driver (PA11=RX, PA12=TX, ISO1050, 500 kbit/s). */
#pragma once
#include <stdint.h>
#include "bms_types.h"

/* Initialise bxCAN at 500 kbit/s. Call after board_clock_init(). */
void board_can_init(void);

/* Transmit one standard CAN frame (11-bit ID, 0–8 data bytes).
 * Polls for a free mailbox; returns BMS_ERR_TIMEOUT if none free within ~1 ms.
 * Safe to call from the main loop — does not block for arbitration. */
BmsResult board_can_send(uint32_t id, const uint8_t *data, uint8_t len);
