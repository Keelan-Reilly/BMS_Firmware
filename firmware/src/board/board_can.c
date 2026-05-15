/* board_can.c — CAN stub. */
#include "board_can.h"
#include "board_pins.h"
#include "bms_hal.h"

void board_can_init(void) {
    /* PA11 CAN_RX, PA12 CAN_TX → AF9 */
    CAN_PORT->MODER &= ~(0xFu << 22);
    CAN_PORT->MODER |= (GPIO_MODER_AF << 22) | (GPIO_MODER_AF << 24);
    CAN_PORT->AFR[1] &= ~(0xFFu << 12);
    CAN_PORT->AFR[1] |= (CAN_AF << 12) | (CAN_AF << 16);
    /* CAN peripheral init: STUB */
}

BmsResult board_can_send(uint32_t id, const uint8_t *data, uint8_t len) {
    (void)id; (void)data; (void)len;
    return BMS_ERR_NOT_SUPPORTED; /* STUB */
}
