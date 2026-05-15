/* board_can.h — CAN bus stub (PA11=RX, PA12=TX, ISO1050). */
#pragma once
#include <stdint.h>
#include "bms_types.h"

/* Init CAN peripheral at 500 kbit/s. */
void board_can_init(void);

/* Transmit one CAN frame. Returns BMS_ERR_NOT_SUPPORTED until implemented. */
BmsResult board_can_send(uint32_t id, const uint8_t *data, uint8_t len);
