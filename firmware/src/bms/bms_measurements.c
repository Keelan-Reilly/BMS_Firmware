/* bms_measurements.c — periodic measurement acquisition.
 *
 * Temperature measurement uses ADCV on the TEMP chain (C-inputs), not ADAX.
 * Per hardware contract §7: sensors are measured on C-inputs via S-output bias.
 *
 * Enepaq V-T table: PLACEHOLDER — populate from Sony-Murata VTC6 datasheet
 * once hardware is confirmed.  All temp readings remain INVALID until table
 * is populated.  See OQ-TMP in docs/01_hardware_contract.md.
 */
#include "bms_measurements.h"
#include "bms_config.h"
#include "bms_faults.h"
#include "bms_diagnostics.h"
#include "ltc6812.h"
#include "isl28022.h"
#include "board_adc.h"
#include "board_clock.h"
#include "board_pins.h"
#include <string.h>
#include <limits.h>

static CellSnapshot    s_cells;
static TempSnapshot    s_temps;
static PackMeasurement s_pack;

/* ── Enepaq V-T lookup table ──────────────────────────────────────────────── */
/* Units: voltage in mV (100µV LSB × raw / 10), temperature in °C×10.
 * Monotonically decreasing voltage with increasing temperature.
 *
 * OQ-TMP: replace with actual Enepaq/Sony-Murata VTC6 V-T breakpoints.
 * Until populated, ENEPAQ_TABLE_POPULATED must remain 0. */
#define ENEPAQ_TABLE_POPULATED  0

#if ENEPAQ_TABLE_POPULATED
typedef struct { uint16_t mv; int16_t cx10; } VTPoint;
static const VTPoint k_enepaq_vt[] = {
    /* { voltage_mv, temperature_cx10 } — sorted by mv DESCENDING */
    /* TODO: populate from datasheet */
};
#define ENEPAQ_VT_COUNT  (sizeof(k_enepaq_vt) / sizeof(k_enepaq_vt[0]))
#endif

/* Convert a measured voltage (in mV, 100µV LSB from LTC6812 raw/10) to °C×10.
 * Returns TEMP_INVALID_CX10 if voltage is outside the table range or table
 * is not yet populated. */
static int16_t enepaq_voltage_to_cx10(uint16_t mv) {
#if ENEPAQ_TABLE_POPULATED
    if (ENEPAQ_VT_COUNT < 2) { return TEMP_INVALID_CX10; }
    if (mv > k_enepaq_vt[0].mv || mv < k_enepaq_vt[ENEPAQ_VT_COUNT - 1].mv) {
        return TEMP_INVALID_CX10;
    }
    for (uint8_t i = 0; i < ENEPAQ_VT_COUNT - 1u; i++) {
        if (mv <= k_enepaq_vt[i].mv && mv >= k_enepaq_vt[i + 1u].mv) {
            /* Linear interpolation between breakpoints */
            int32_t dv = (int32_t)k_enepaq_vt[i].mv - (int32_t)k_enepaq_vt[i + 1u].mv;
            int32_t dt = (int32_t)k_enepaq_vt[i + 1u].cx10 - (int32_t)k_enepaq_vt[i].cx10;
            int32_t offset = (int32_t)k_enepaq_vt[i].mv - (int32_t)mv;
            int32_t cx10 = (int32_t)k_enepaq_vt[i].cx10 + (offset * dt) / dv;
            return (int16_t)cx10;
        }
    }
    return TEMP_INVALID_CX10;
#else
    (void)mv;
    return TEMP_INVALID_CX10; /* OQ-TMP: Enepaq V-T table not yet populated */
#endif
}

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
        bms_faults_clear_pec_counter(BMS_CHAIN_CELL);
    } else {
        s_cells.overall = MEAS_ERROR;
        for (uint8_t i = 0; i < TOTAL_CELL_COUNT; i++) { s_cells.valid[i] = false; }
        if (r == BMS_ERR_PEC) { bms_faults_report_pec_error(BMS_CHAIN_CELL); }
    }

    return r;
}

/* ── Temperature cycle ────────────────────────────────────────────────────── */
/* Temperature sensors are on C-inputs of the TEMP chain. Measurement uses
 * ADCV + RDCVA-RDCVE (same as cell measurement), not ADAX + RDAUX.
 * S-outputs must be cleared on success AND every error path. */
BmsResult bms_measurements_run_temp_cycle(void) {
    const BmsConfig *cfg = bms_config_get();
    BmsResult r;

    /* 1. Assert S-outputs for all configured temp sensor channels.
     *    S-outputs are numbered per-IC; use full mask for all 15 channels. */
    r = ltc6812_temp_chain_set_sensor_bias(BMS_CHAIN_TEMP, TEMP_IC_COUNT, 0x7FFFu);
    if (r != BMS_OK) {
        ltc6812_temp_chain_clear_s_outputs(BMS_CHAIN_TEMP, TEMP_IC_COUNT);
        s_temps.overall = MEAS_ERROR;
        bms_faults_report_pec_error(BMS_CHAIN_TEMP);
        return r;
    }

    /* 2. Wait for sensor voltage to settle */
    board_clock_delay_ms(cfg->temp_settle_time_ms);

    /* 3. Run ADCV on TEMP chain and read RDCVA-RDCVE (C-input voltages) */
    uint16_t raw_mv[TEMP_IC_COUNT][CELLS_PER_IC];
    bool pec_ok[TEMP_IC_COUNT];
    r = ltc6812_read_cells(BMS_CHAIN_TEMP, TEMP_IC_COUNT, raw_mv, pec_ok);

    /* 4. ALWAYS clear S-outputs regardless of result */
    BmsResult clear_r = ltc6812_temp_chain_clear_s_outputs(BMS_CHAIN_TEMP, TEMP_IC_COUNT);

    s_temps.timestamp_ms = board_clock_get_ms();

    if (r == BMS_OK) {
        /* 5. Convert each channel voltage to temperature */
        for (uint8_t ic = 0; ic < TEMP_IC_COUNT; ic++) {
            for (uint8_t ch = 0; ch < TEMPS_PER_IC; ch++) {
                uint8_t idx = ic * TEMPS_PER_IC + ch;
                if (pec_ok[ic]) {
                    int16_t cx10 = enepaq_voltage_to_cx10(raw_mv[ic][ch]);
                    s_temps.cx10[idx]  = cx10;
                    s_temps.valid[idx] = (cx10 != TEMP_INVALID_CX10);
                } else {
                    s_temps.cx10[idx]  = TEMP_INVALID_CX10;
                    s_temps.valid[idx] = false;
                }
            }
        }
        /* Overall validity: valid only if all required sensors converted */
        s_temps.overall = MEAS_VALID; /* caller (bms_faults) checks coverage */
        bms_faults_clear_pec_counter(BMS_CHAIN_TEMP);
    } else {
        s_temps.overall = MEAS_ERROR;
        for (uint8_t i = 0; i < TOTAL_TEMP_COUNT; i++) {
            s_temps.cx10[i]  = TEMP_INVALID_CX10;
            s_temps.valid[i] = false;
        }
        if (r == BMS_ERR_PEC) { bms_faults_report_pec_error(BMS_CHAIN_TEMP); }
    }

    return (r != BMS_OK) ? r : clear_r;
}

/* ── Pack cycle (Vbat+I via ISL28022, Vpack via ADC1/PA1) ────────────────── */
BmsResult bms_measurements_run_pack_cycle(void) {
    const BmsConfig *cfg = bms_config_get();

    /* --- ISL28022: Vbat and I_batt --- */
    int32_t vbus_raw_mv = INT32_MIN;
    int32_t vshunt_raw_uv = 0;
    bool vbat_valid  = false;
    bool ibatt_valid = false;

    BmsResult r_isl = isl28022_read(&vbus_raw_mv, &vshunt_raw_uv);
    if (r_isl == BMS_OK) {
        vbat_valid  = true;
        ibatt_valid = true;
        bms_faults_clear_i2c_error();
    } else {
        bms_faults_report_i2c_error();
        bms_diagnostics_record_i2c_error();
    }

    /* Apply config-provided calibration: vbat_mv = raw * gain/1000 + offset */
    int32_t vbat_mv = INT32_MIN;
    if (vbat_valid) {
        vbat_mv = (vbus_raw_mv * (int32_t)cfg->vbat_gain_x1000) / 1000 +
                  (int32_t)cfg->vbat_offset_mv;
    }

    /* Current: vshunt_raw_uv / shunt_mohm → mA, then apply gain/offset.
     * current_gain_x1000 encodes (1000 / shunt_resistance_mohm) combined with
     * any additional calibration factor. i_batt_ma = raw_uv * gain / 1000000 + offset. */
    int32_t i_batt_ma = 0;
    if (ibatt_valid) {
        i_batt_ma = (vshunt_raw_uv * (int32_t)cfg->current_gain_x1000) / 1000000 +
                    (int32_t)cfg->current_offset_ma;
    }

    /* --- ADC1: Vpack on PA1 (ADC1_IN2) --- */
    uint16_t adc_raw = 0;
    bool vpack_valid = false;
    int32_t vpack_mv = INT32_MIN;

    BmsResult r_adc = board_adc_read_raw(&adc_raw);
    if (r_adc == BMS_OK) {
        vpack_valid = true;
        /* Convert 12-bit raw to mV using VREF, then apply gain/offset. */
        int32_t raw_mv = ((int32_t)adc_raw * (int32_t)VPACK_VREF_MV) / 4096;
        vpack_mv = (raw_mv * (int32_t)cfg->vpack_gain_x1000) / 1000 +
                   (int32_t)cfg->vpack_offset_mv;
    }

    bms_measurements_update_pack(vbat_mv, vpack_mv, i_batt_ma,
                                  vbat_valid, vpack_valid, ibatt_valid);

    /* Return worst error so caller can decide on retry. */
    if (r_isl != BMS_OK) { return r_isl; }
    return r_adc;
}

/* ── Pack measurement update (called from ISL/ADC drivers on success) ─────── */
void bms_measurements_update_pack(int32_t vbat_mv, int32_t vpack_mv,
                                   int32_t i_batt_ma,
                                   bool vbat_valid, bool vpack_valid,
                                   bool i_batt_valid) {
    s_pack.vbat_mv      = vbat_mv;
    s_pack.vpack_mv     = vpack_mv;
    s_pack.i_batt_ma    = i_batt_ma;
    s_pack.vbat_valid   = vbat_valid;
    s_pack.vpack_valid  = vpack_valid;
    s_pack.i_batt_valid = i_batt_valid;
    s_pack.timestamp_ms = board_clock_get_ms();
}

/* ── Accessors ────────────────────────────────────────────────────────────── */
const CellSnapshot    *bms_measurements_get_cells(void) { return &s_cells; }
const TempSnapshot    *bms_measurements_get_temps(void) { return &s_temps; }
const PackMeasurement *bms_measurements_get_pack(void)  { return &s_pack;  }
