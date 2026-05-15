/* bms_measurements.c — measurement acquisition stubs.
 *
 * STUB: Cell and pack cycles return INVALID snapshots.
 * Temperature cycle enforces the S-output clear-on-exit invariant.
 */
#include "bms_measurements.h"
#include "bms_config.h"
#include "ltc6812.h"
#include "board_clock.h"
#include <string.h>

static CellSnapshot    s_cells;
static TempSnapshot    s_temps;
static PackMeasurement s_pack;

/* ── Cell cycle ───────────────────────────────────────────────────────────── */
BmsResult bms_measurements_run_cell_cycle(void) {
    uint16_t raw_mv[CELL_IC_COUNT][CELLS_PER_IC];
    bool pec_ok[CELL_IC_COUNT];

    BmsResult r = ltc6812_read_cells(BMS_CHAIN_CELL, CELL_IC_COUNT, raw_mv, pec_ok);

    s_cells.timestamp_ms = board_clock_get_ms();

    if (r == BMS_OK) {
        for (uint8_t ic = 0; ic < CELL_IC_COUNT; ic++) {
            for (uint8_t c = 0; c < CELLS_PER_IC; c++) {
                uint8_t idx = ic * CELLS_PER_IC + c;
                s_cells.mv[idx]    = raw_mv[ic][c];
                s_cells.valid[idx] = pec_ok[ic];
            }
        }
        s_cells.overall = MEAS_VALID;
    } else {
        s_cells.overall = MEAS_ERROR;
        for (uint8_t i = 0; i < TOTAL_CELL_COUNT; i++) { s_cells.valid[i] = false; }
    }

    return r;
}

/* ── Temperature cycle ────────────────────────────────────────────────────── */
/* S-output clear is guaranteed in BOTH success and error paths. */
BmsResult bms_measurements_run_temp_cycle(void) {
    const BmsConfig *cfg = bms_config_get();
    BmsResult r;

    /* Assert S-outputs (all channels enabled for now) */
    r = ltc6812_temp_chain_set_sensor_bias(BMS_CHAIN_TEMP, TEMP_IC_COUNT, 0x01FFu);
    if (r != BMS_OK) {
        /* Even on sensor-bias failure, attempt to clear S-outputs */
        ltc6812_temp_chain_clear_s_outputs(BMS_CHAIN_TEMP, TEMP_IC_COUNT);
        s_temps.overall = MEAS_ERROR;
        return r;
    }

    /* Wait for sensor to settle */
    board_clock_delay_ms(cfg->temp_settle_time_ms);

    /* Read auxiliary ADC channels */
    uint16_t raw_adc[TEMP_IC_COUNT][9];
    bool pec_ok[TEMP_IC_COUNT];
    r = ltc6812_read_aux(BMS_CHAIN_TEMP, TEMP_IC_COUNT, raw_adc, pec_ok);

    /* ALWAYS clear S-outputs — success or failure */
    BmsResult clear_r = ltc6812_temp_chain_clear_s_outputs(BMS_CHAIN_TEMP, TEMP_IC_COUNT);

    s_temps.timestamp_ms = board_clock_get_ms();

    if (r == BMS_OK) {
        /* TODO: convert raw ADC → °C×10 using Enepaq V-T table (OQ-TMP) */
        /* STUB: mark all temps invalid until conversion implemented */
        for (uint8_t i = 0; i < TOTAL_TEMP_COUNT; i++) {
            s_temps.cx10[i] = TEMP_INVALID_CX10;
            s_temps.valid[i] = false;
        }
        s_temps.overall = MEAS_INVALID; /* STUB */
    } else {
        s_temps.overall = MEAS_ERROR;
        for (uint8_t i = 0; i < TOTAL_TEMP_COUNT; i++) {
            s_temps.cx10[i]  = TEMP_INVALID_CX10;
            s_temps.valid[i] = false;
        }
    }

    return (r != BMS_OK) ? r : clear_r;
}

/* ── Pack cycle ───────────────────────────────────────────────────────────── */
BmsResult bms_measurements_run_pack_cycle(void) {
    /* STUB: ISL28022 and ADC drivers not yet implemented */
    s_pack.vbat_mv    = INT32_MIN;
    s_pack.vpack_mv   = INT32_MIN;
    s_pack.i_batt_ma  = 0;
    s_pack.vbat_valid = false;
    s_pack.vpack_valid = false;
    s_pack.i_batt_valid = false;
    s_pack.timestamp_ms = board_clock_get_ms();
    return BMS_ERR_NOT_SUPPORTED;
}

/* ── Accessors ────────────────────────────────────────────────────────────── */
const CellSnapshot    *bms_measurements_get_cells(void) { return &s_cells; }
const TempSnapshot    *bms_measurements_get_temps(void) { return &s_temps; }
const PackMeasurement *bms_measurements_get_pack(void)  { return &s_pack;  }
