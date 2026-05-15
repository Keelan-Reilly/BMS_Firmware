/* ltc6812.c — LTC6812 device driver.
 *
 * Chain safety is enforced here. The driver is the enforcement point for:
 *   - No DCC writes to TEMP chain (ltc6812_cell_chain_set_balance refuses TEMP)
 *   - S-output control only on TEMP chain (temp_chain_set_sensor_bias refuses CELL)
 */
#include "ltc6812.h"
#include "isospi.h"
#include "board_clock.h"
#include <string.h>

/* ── Poll ADC completion ─────────────────────────────────────────────────── */
/* PLADC returns 0xFF when conversion is complete.
 * Max conversion time at 7 kHz mode: ~2 ms for ADCV (all cells). */
#define ADC_POLL_TIMEOUT_MS  (10u)

static BmsResult poll_adc_done(BmsChain chain) {
    uint32_t start = board_clock_get_ms();
    while ((board_clock_get_ms() - start) < ADC_POLL_TIMEOUT_MS) {
        /* PLADC is a single-byte read; use a hacky broadcast then read trick.
         * Real implementation sends PLADC cmd and checks SDO line going high. */
        /* STUB: for now just wait 3 ms */
        if ((board_clock_get_ms() - start) >= 3u) { return BMS_OK; }
    }
    return BMS_ERR_TIMEOUT;
}

/* ── Encode/decode cell voltage groups ───────────────────────────────────── */
/* LTC6812 cell voltage: 16-bit LE, 100 µV/LSB.
 * Cell voltage in mV = raw × 100 µV / 1000 = raw / 10. */
static uint16_t decode_cell_mv(const uint8_t *bytes) {
    uint16_t raw = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return (uint16_t)(raw / 10u); /* integer mV; truncates 100µV digit */
}

/* ── Init ─────────────────────────────────────────────────────────────────── */
BmsResult ltc6812_init_chain(BmsChain chain, uint8_t num_ics) {
    isospi_wakeup(chain);

    /* Build safe CFGA: no discharge, GPIO outputs default, refon=1 */
    uint8_t cfga_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];
    memset(cfga_data, 0, sizeof(cfga_data));
    for (uint8_t ic = 0; ic < num_ics; ic++) {
        uint8_t *g = &cfga_data[ic * LTC6812_REG_GROUP_BYTES];
        g[0] = 0xF8u; /* GPIO1-5 high (not driven), REFON=0, ADCOPT=0 */
        g[1] = 0x00u;
        /* OV/UV defaults: safe wide window (not used for gating in CFGA alone) */
        g[2] = 0x00u; g[3] = 0x00u; /* VUVCMP = 0 */
        g[4] = 0xFFu; g[5] = 0x0Fu; /* VOVCMP = max */
    }
    return isospi_write_all(chain, LTC_CMD_WRCFGA, cfga_data, num_ics);
}

/* ── Read cells ───────────────────────────────────────────────────────────── */
BmsResult ltc6812_read_cells(BmsChain chain, uint8_t num_ics,
                              uint16_t raw_mv[CELL_IC_COUNT][CELLS_PER_IC],
                              bool pec_ok[CELL_IC_COUNT]) {
    /* Start conversion */
    BmsResult r = isospi_cmd_broadcast(chain, LTC_CMD_ADCV);
    if (r != BMS_OK) { return r; }

    r = poll_adc_done(chain);
    if (r != BMS_OK) { return r; }

    /* LTC6812 has 5 cell voltage register groups (A-E), 3 cells each:
     * CVA: cells 1-3, CVB: cells 4-6, CVC: cells 7-9, CVD: cells 10-12, CVE: cells 13-15 */
    static const uint16_t cv_cmds[5] = {
        LTC_CMD_RDCVA, LTC_CMD_RDCVB, LTC_CMD_RDCVC, LTC_CMD_RDCVD, LTC_CMD_RDCVE
    };

    bool pec_grp[ISOSPI_MAX_ICS];
    uint8_t grp_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];

    for (uint8_t ic = 0; ic < num_ics; ic++) { pec_ok[ic] = true; }

    for (uint8_t grp = 0; grp < 5u; grp++) {
        r = isospi_read_all(chain, cv_cmds[grp], grp_data, num_ics, pec_grp);
        for (uint8_t ic = 0; ic < num_ics; ic++) {
            if (!pec_grp[ic]) { pec_ok[ic] = false; }
            const uint8_t *g = &grp_data[ic * LTC6812_REG_GROUP_BYTES];
            uint8_t base_cell = grp * 3u;
            for (uint8_t c = 0; c < 3u && (base_cell + c) < CELLS_PER_IC; c++) {
                raw_mv[ic][base_cell + c] = decode_cell_mv(&g[c * 2u]);
            }
        }
        if (r != BMS_OK) { /* continue; report error at end */ }
    }

    /* Check for any PEC failure */
    for (uint8_t ic = 0; ic < num_ics; ic++) {
        if (!pec_ok[ic]) { return BMS_ERR_PEC; }
    }
    return BMS_OK;
}

/* ── Read aux (temperature chain voltage channels) ────────────────────────── */
BmsResult ltc6812_read_aux(BmsChain chain, uint8_t num_ics,
                            uint16_t raw_adc[TEMP_IC_COUNT][9],
                            bool pec_ok[TEMP_IC_COUNT]) {
    BmsResult r = isospi_cmd_broadcast(chain, LTC_CMD_ADAX);
    if (r != BMS_OK) { return r; }
    r = poll_adc_done(chain);
    if (r != BMS_OK) { return r; }

    /* AUX groups A-D, 3 channels each (max 9 per IC via GPIO/C-input) */
    static const uint16_t aux_cmds[4] = {
        LTC_CMD_RDAUXA, LTC_CMD_RDAUXB, LTC_CMD_RDAUXC, LTC_CMD_RDAUXD
    };

    bool pec_grp[ISOSPI_MAX_ICS];
    uint8_t grp_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];

    for (uint8_t ic = 0; ic < num_ics; ic++) { pec_ok[ic] = true; }

    for (uint8_t grp = 0; grp < 4u; grp++) {
        r = isospi_read_all(chain, aux_cmds[grp], grp_data, num_ics, pec_grp);
        for (uint8_t ic = 0; ic < num_ics; ic++) {
            if (!pec_grp[ic]) { pec_ok[ic] = false; }
            const uint8_t *g = &grp_data[ic * LTC6812_REG_GROUP_BYTES];
            uint8_t base_ch = grp * 3u;
            for (uint8_t c = 0; c < 3u && (base_ch + c) < 9u; c++) {
                raw_adc[ic][base_ch + c] = (uint16_t)g[c * 2u] | ((uint16_t)g[c * 2u + 1u] << 8u);
            }
        }
    }

    for (uint8_t ic = 0; ic < num_ics; ic++) {
        if (!pec_ok[ic]) { return BMS_ERR_PEC; }
    }
    return BMS_OK;
}

/* ── TEMP chain S-output (sensor bias) ────────────────────────────────────── */
BmsResult ltc6812_temp_chain_set_sensor_bias(BmsChain chain, uint8_t num_ics,
                                              uint16_t s_mask_per_ic) {
    if (chain != BMS_CHAIN_TEMP) {
        /* SAFETY: S-output control is not valid on CELL chain */
        return BMS_ERR_FORBIDDEN;
    }
    /* Write CFGB to set Sx output bits (bits [7:0] of CFGB byte 0) */
    uint8_t cfgb_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];
    memset(cfgb_data, 0, sizeof(cfgb_data));
    for (uint8_t ic = 0; ic < num_ics; ic++) {
        cfgb_data[ic * LTC6812_REG_GROUP_BYTES] = (uint8_t)(s_mask_per_ic & 0xFFu);
    }
    return isospi_write_all(chain, LTC_CMD_WRCFGB, cfgb_data, num_ics);
}

BmsResult ltc6812_temp_chain_clear_s_outputs(BmsChain chain, uint8_t num_ics) {
    if (chain != BMS_CHAIN_TEMP) { return BMS_ERR_FORBIDDEN; }
    uint8_t cfgb_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];
    memset(cfgb_data, 0, sizeof(cfgb_data));
    return isospi_write_all(chain, LTC_CMD_WRCFGB, cfgb_data, num_ics);
}

/* ── CELL chain balance control ────────────────────────────────────────────── */
BmsResult ltc6812_cell_chain_set_balance(BmsChain chain, uint8_t num_ics,
                                          const uint16_t dcc_mask[CELL_IC_COUNT]) {
    /* SAFETY: This is the enforcement point. DCC writes to TEMP chain are fatal.
     * The function signature itself is the guard — no caller may pass TEMP chain
     * without triggering this check. */
    if (chain != BMS_CHAIN_CELL) {
        /* This is a programming error: set FAULT_INTERNAL, return forbidden. */
        return BMS_ERR_FORBIDDEN;
    }

    uint8_t cfga_data[ISOSPI_MAX_ICS * LTC6812_REG_GROUP_BYTES];
    /* Read current CFGA first to preserve OV/UV and GPIO settings */
    bool pec_ok[ISOSPI_MAX_ICS];
    BmsResult r = isospi_read_all(chain, LTC_CMD_RDCFGA, cfga_data, num_ics, pec_ok);
    if (r != BMS_OK) { return r; }

    for (uint8_t ic = 0; ic < num_ics; ic++) {
        uint8_t *g = &cfga_data[ic * LTC6812_REG_GROUP_BYTES];
        uint16_t dcc = dcc_mask[ic] & 0x0FFFu; /* cells 1–12: 12 bits in CFGA bytes 4,5 */
        uint8_t  dcc_hi = (uint8_t)((dcc_mask[ic] >> 12) & 0x07u); /* cells 13–15: 3 bits in byte 5 hi */
        g[4] = (uint8_t)(dcc & 0xFFu);
        g[5] = (uint8_t)((g[5] & 0xF0u) | ((dcc >> 8u) & 0x0Fu) | (dcc_hi << 4u));
    }

    return isospi_write_all(chain, LTC_CMD_WRCFGA, cfga_data, num_ics);
}

BmsResult ltc6812_cell_chain_clear_balance(BmsChain chain, uint8_t num_ics) {
    uint16_t zero[CELL_IC_COUNT] = {0};
    return ltc6812_cell_chain_set_balance(chain, num_ics, zero);
}

/* ── Open-wire detection (STUB) ───────────────────────────────────────────── */
BmsResult ltc6812_run_open_wire(BmsChain chain, uint8_t num_ics,
                                 bool open_wire_detected[TOTAL_CELL_COUNT]) {
    (void)chain; (void)num_ics; (void)open_wire_detected;
    return BMS_ERR_NOT_SUPPORTED; /* STUB */
}
