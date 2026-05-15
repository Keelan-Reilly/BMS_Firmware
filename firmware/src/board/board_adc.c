/* board_adc.c — ADC1 stub for PA1/channel-2 Vpack measurement. STUB. */
#include "board_adc.h"
#include "board_pins.h"
#include "bms_hal.h"

void board_adc_init(void) {
    /* PA1 → analog mode */
    VPACK_ADC_PORT->MODER |= (GPIO_MODER_ANALOG << (VPACK_ADC_PIN * 2u));
    /* ADC1 peripheral init: STUB */
}

BmsResult board_adc_read_raw(uint16_t *raw_out) {
    (void)raw_out;
    return BMS_ERR_NOT_SUPPORTED; /* STUB */
}
