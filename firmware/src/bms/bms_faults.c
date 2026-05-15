/* bms_faults.c — fault evaluation, latching, and clearing. */
#include "bms_faults.h"
#include "bms_config.h"
#include "board_clock.h"

static uint64_t s_active;
static uint64_t s_latched;

static inline void set_active(FaultBit bit, bool cond) {
    if (cond) {
        s_active  |= FAULT_MASK(bit);
        s_latched |= FAULT_MASK(bit);
    } else {
        s_active &= ~FAULT_MASK(bit);
    }
}

void bms_faults_evaluate(const CellSnapshot    *cells,
                          const TempSnapshot    *temps,
                          const PackMeasurement *pack,
                          const BmsConfig       *cfg) {
    uint32_t now = board_clock_get_ms();

    /* ── Measurement validity / staleness ─────────────────────────────────── */
    bool cells_stale = (now - cells->timestamp_ms) > cfg->stale_data_timeout_ms;
    bool temps_stale = (now - temps->timestamp_ms) > cfg->stale_data_timeout_ms;
    bool pack_stale  = (now - pack->timestamp_ms)  > cfg->stale_data_timeout_ms;

    set_active(FAULT_BIT_STALE_CELLS, cells_stale || cells->overall == MEAS_ERROR);
    set_active(FAULT_BIT_STALE_TEMPS, temps_stale || temps->overall == MEAS_ERROR);
    set_active(FAULT_BIT_STALE_PACK,  pack_stale  || (!pack->vbat_valid && !pack->vpack_valid));

    set_active(FAULT_BIT_CELL_MEASUREMENT, cells->overall == MEAS_ERROR);
    set_active(FAULT_BIT_TEMP_MEASUREMENT, temps->overall == MEAS_ERROR);
    set_active(FAULT_BIT_VBAT_INVALID,  !pack->vbat_valid);
    set_active(FAULT_BIT_VPACK_INVALID, !pack->vpack_valid);

    /* ── Cell voltage faults ──────────────────────────────────────────────── */
    bool any_uv_hard = false, any_uv_soft = false;
    bool any_ov_hard = false, any_ov_soft = false;
    if (!cells_stale && cells->overall != MEAS_ERROR) {
        for (uint8_t i = 0; i < TOTAL_CELL_COUNT; i++) {
            if (!cells->valid[i]) { continue; }
            uint16_t mv = cells->mv[i];
            /* check required_cell_mask: skip cells not in mask */
            uint8_t byte = i / 8u;
            uint8_t bit  = i % 8u;
            if (!(cfg->required_cell_mask[byte] & (1u << bit))) { continue; }
            if (mv < cfg->cell_uv_hard_mv) { any_uv_hard = true; }
            if (mv < cfg->cell_uv_soft_mv) { any_uv_soft = true; }
            if (mv > cfg->cell_ov_hard_mv) { any_ov_hard = true; }
            if (mv > cfg->cell_ov_soft_mv) { any_ov_soft = true; }
        }
    }
    set_active(FAULT_BIT_CELL_UV_HARD, any_uv_hard);
    set_active(FAULT_BIT_CELL_UV_SOFT, any_uv_soft);
    set_active(FAULT_BIT_CELL_OV_HARD, any_ov_hard);
    set_active(FAULT_BIT_CELL_OV_SOFT, any_ov_soft);

    /* ── Temperature faults ───────────────────────────────────────────────── */
    bool temp_charge_warn = false,    temp_charge_hard = false;
    bool temp_discharge_warn = false, temp_discharge_hard = false;
    bool temp_cold_charge = false,    temp_cold_discharge = false;
    if (!temps_stale && temps->overall != MEAS_ERROR) {
        for (uint8_t i = 0; i < TOTAL_TEMP_COUNT; i++) {
            if (!temps->valid[i] || temps->cx10[i] == TEMP_INVALID_CX10) { continue; }
            uint8_t byte = i / 8u; uint8_t tbit = i % 8u;
            if (!(cfg->required_temp_mask[byte] & (1u << tbit))) { continue; }
            int16_t t = temps->cx10[i];
            if (t >= cfg->temp_charge_warn_cx10)    { temp_charge_warn = true; }
            if (t >= cfg->temp_charge_hard_cx10)    { temp_charge_hard = true; }
            if (t >= cfg->temp_discharge_warn_cx10) { temp_discharge_warn = true; }
            if (t >= cfg->temp_discharge_hard_cx10) { temp_discharge_hard = true; }
            if (t < cfg->temp_cold_charge_limit_cx10)    { temp_cold_charge = true; }
            if (t < cfg->temp_cold_discharge_limit_cx10) { temp_cold_discharge = true; }
        }
    }
    set_active(FAULT_BIT_TEMP_HIGH_CHARGE,    temp_charge_hard);
    set_active(FAULT_BIT_TEMP_HIGH_DISCHARGE, temp_discharge_hard);
    set_active(FAULT_BIT_TEMP_COLD_CHARGE,    temp_cold_charge);
    set_active(FAULT_BIT_TEMP_COLD_DISCHARGE, temp_cold_discharge);
    (void)temp_charge_warn; (void)temp_discharge_warn; /* warnings for future use */

    /* ── Current faults ───────────────────────────────────────────────────── */
    if (pack->i_batt_valid) {
        uint32_t abs_i = (pack->i_batt_ma >= 0) ? (uint32_t)pack->i_batt_ma
                                                  : (uint32_t)(-pack->i_batt_ma);
        set_active(FAULT_BIT_OVERCURRENT_HARD, abs_i > cfg->overcurrent_hard_ma);
        set_active(FAULT_BIT_OVERCURRENT_WARN, abs_i > cfg->overcurrent_warn_ma);
    } else {
        set_active(FAULT_BIT_OVERCURRENT_HARD, false);
        set_active(FAULT_BIT_OVERCURRENT_WARN, false);
    }
}

uint64_t bms_faults_get_active(void)  { return s_active;  }
uint64_t bms_faults_get_latched(void) { return s_latched; }

uint64_t bms_faults_clear_latched(uint64_t mask) {
    uint64_t clearable = mask & s_latched & ~s_active;
    s_latched &= ~clearable;
    return clearable;
}

void bms_faults_set(FaultBit bit) {
    s_active  |= FAULT_MASK(bit);
    s_latched |= FAULT_MASK(bit);
}
