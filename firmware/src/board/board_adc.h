/* board_adc.h — ADC1 stub for Vpack measurement (PA1, channel 2). */
#pragma once
#include <stdint.h>
#include "bms_types.h"

/* Init ADC1 for single-channel conversion on PA1 (ADC1_IN2). */
void board_adc_init(void);

/* Trigger a single conversion and return 12-bit raw result.
 * Returns BMS_ERR_TIMEOUT if conversion does not complete. */
BmsResult board_adc_read_raw(uint16_t *raw_out);
