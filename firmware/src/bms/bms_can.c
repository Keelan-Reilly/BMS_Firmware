/* bms_can.c — CAN message composition and periodic transmission.
 *
 * All frames use standard 11-bit IDs. base_id comes from config.can_base_id
 * (default 0x500). Offsets are documented in bms_can.h.
 *
 * Cell and temp frames are sent one per main-loop tick to spread bus load.
 * At 10 ms main loop and 19 frames per burst, the full cell set takes 190 ms
 * to complete; each cell is refreshed at ~5 Hz, which is adequate for driving.
 */
#include "bms_can.h"
#include "bms_config.h"
#include "bms_measurements.h"
#include "bms_faults.h"
#include "bms_state.h"
#include "bms_outputs.h"
#include "board_can.h"
#include "board_clock.h"
#include "bms_constants.h"
#include "bms_types.h"
#include <string.h>

/* ── ID offsets from base ─────────────────────────────────────────────────── */
#define CAN_OFF_PACK        0x000u
#define CAN_OFF_FAULTS      0x001u
#define CAN_OFF_CELLS       0x010u   /* 0x010..0x022 — 19 frames */
#define CAN_OFF_TEMPS       0x030u   /* 0x030..0x042 — 19 frames */

#define CELLS_PER_FRAME     4u
#define TEMPS_PER_FRAME     4u
#define CELL_FRAME_COUNT    ((TOTAL_CELL_COUNT + CELLS_PER_FRAME - 1u) / CELLS_PER_FRAME)
#define TEMP_FRAME_COUNT    ((TOTAL_TEMP_COUNT + TEMPS_PER_FRAME - 1u) / TEMPS_PER_FRAME)

/* ── Periods ──────────────────────────────────────────────────────────────── */
#define PACK_PERIOD_MS      100u
#define FAULT_PERIOD_MS     100u

/* ── Module state ─────────────────────────────────────────────────────────── */
static uint32_t s_last_pack_ms;
static uint32_t s_last_fault_ms;
static uint8_t  s_next_cell_frame;   /* 0..18, incremented each tick */
static uint8_t  s_next_temp_frame;
static uint32_t s_last_cell_burst_ms;
static uint32_t s_last_temp_burst_ms;

#define CELL_BURST_PERIOD_MS   100u
#define TEMP_BURST_PERIOD_MS   500u

void bms_can_init(void) {
    uint32_t now = board_clock_get_ms();
    s_last_pack_ms       = now;
    s_last_fault_ms      = now;
    s_last_cell_burst_ms = now;
    s_last_temp_burst_ms = now;
    s_next_cell_frame    = 0u;
    s_next_temp_frame    = 0u;
}

/* ── Frame builders ───────────────────────────────────────────────────────── */

static void _send_pack(uint16_t base) {
    const PackMeasurement *pack = bms_measurements_get_pack();
    BmsState state = bms_state_get();
    uint8_t outputs = bms_outputs_get_state();

    /* Vbat: mV, 0xFFFF = invalid */
    uint16_t vbat_raw = (pack->vbat_valid && pack->vbat_mv >= 0)
                        ? (uint16_t)pack->vbat_mv : 0xFFFFu;

    /* Vpack: mV, 0xFFFF = invalid */
    uint16_t vpack_raw = (pack->vpack_valid && pack->vpack_mv >= 0)
                         ? (uint16_t)pack->vpack_mv : 0xFFFFu;

    /* Current: 100 mA/LSB, positive = discharge. 0x7FFF = invalid. */
    int16_t i_raw = 0x7FFF;
    if (pack->i_batt_valid) {
        int32_t scaled = pack->i_batt_ma / 100;
        if (scaled >  32766) { scaled =  32766; }
        if (scaled < -32767) { scaled = -32767; }
        i_raw = (int16_t)scaled;
    }

    /* flags byte: bits[3:0]=outputs, bits[6:4]=measurement validity */
    uint8_t meas_flags = (pack->vbat_valid  ? 0x10u : 0u) |
                         (pack->vpack_valid ? 0x20u : 0u) |
                         (pack->i_batt_valid ? 0x40u : 0u);
    uint8_t flags = (outputs & 0x0Fu) | meas_flags;

    uint8_t buf[8];
    buf[0] = (uint8_t)(vbat_raw & 0xFFu);
    buf[1] = (uint8_t)(vbat_raw >> 8u);
    buf[2] = (uint8_t)(vpack_raw & 0xFFu);
    buf[3] = (uint8_t)(vpack_raw >> 8u);
    buf[4] = (uint8_t)((uint16_t)i_raw & 0xFFu);
    buf[5] = (uint8_t)((uint16_t)i_raw >> 8u);
    buf[6] = (uint8_t)state;
    buf[7] = flags;

    board_can_send(base + CAN_OFF_PACK, buf, 8u);
}

static void _send_faults(uint16_t base) {
    uint32_t active  = (uint32_t)(bms_faults_get_active()  & 0xFFFFFFFFu);
    uint32_t latched = (uint32_t)(bms_faults_get_latched() & 0xFFFFFFFFu);

    uint8_t buf[8];
    buf[0] = (uint8_t)(active & 0xFFu);
    buf[1] = (uint8_t)((active >>  8u) & 0xFFu);
    buf[2] = (uint8_t)((active >> 16u) & 0xFFu);
    buf[3] = (uint8_t)((active >> 24u) & 0xFFu);
    buf[4] = (uint8_t)(latched & 0xFFu);
    buf[5] = (uint8_t)((latched >>  8u) & 0xFFu);
    buf[6] = (uint8_t)((latched >> 16u) & 0xFFu);
    buf[7] = (uint8_t)((latched >> 24u) & 0xFFu);

    board_can_send(base + CAN_OFF_FAULTS, buf, 8u);
}

static void _send_cell_frame(uint16_t base, uint8_t frame_idx) {
    const CellSnapshot *cells = bms_measurements_get_cells();
    uint8_t buf[8];
    for (uint8_t i = 0u; i < CELLS_PER_FRAME; i++) {
        uint8_t cell_idx = frame_idx * CELLS_PER_FRAME + i;
        uint16_t val;
        if (cell_idx < TOTAL_CELL_COUNT && cells->valid[cell_idx]) {
            val = cells->mv[cell_idx];
        } else {
            val = 0xFFFFu;
        }
        buf[i * 2u]      = (uint8_t)(val & 0xFFu);
        buf[i * 2u + 1u] = (uint8_t)(val >> 8u);
    }
    board_can_send(base + CAN_OFF_CELLS + frame_idx, buf, 8u);
}

static void _send_temp_frame(uint16_t base, uint8_t frame_idx) {
    const TempSnapshot *temps = bms_measurements_get_temps();
    uint8_t buf[8];
    for (uint8_t i = 0u; i < TEMPS_PER_FRAME; i++) {
        uint8_t temp_idx = frame_idx * TEMPS_PER_FRAME + i;
        int16_t val;
        if (temp_idx < TOTAL_TEMP_COUNT && temps->valid[temp_idx]) {
            val = temps->cx10[temp_idx];
        } else {
            val = TEMP_INVALID_CX10;
        }
        buf[i * 2u]      = (uint8_t)((uint16_t)val & 0xFFu);
        buf[i * 2u + 1u] = (uint8_t)((uint16_t)val >> 8u);
    }
    board_can_send(base + CAN_OFF_TEMPS + frame_idx, buf, 8u);
}

/* ── Tick ─────────────────────────────────────────────────────────────────── */

void bms_can_tick(void) {
    const BmsConfig *cfg = bms_config_get();
    uint16_t base = cfg->can_base_id;
    uint32_t now  = board_clock_get_ms();

    if ((now - s_last_pack_ms) >= PACK_PERIOD_MS) {
        s_last_pack_ms = now;
        _send_pack(base);
    }

    if ((now - s_last_fault_ms) >= FAULT_PERIOD_MS) {
        s_last_fault_ms = now;
        _send_faults(base);
    }

    /* Cell frames: one per tick during the burst window */
    if (s_next_cell_frame < CELL_FRAME_COUNT) {
        _send_cell_frame(base, s_next_cell_frame);
        s_next_cell_frame++;
    } else if ((now - s_last_cell_burst_ms) >= CELL_BURST_PERIOD_MS) {
        s_last_cell_burst_ms = now;
        s_next_cell_frame = 0u;
    }

    /* Temp frames: one per tick during the burst window */
    if (s_next_temp_frame < TEMP_FRAME_COUNT) {
        _send_temp_frame(base, s_next_temp_frame);
        s_next_temp_frame++;
    } else if ((now - s_last_temp_burst_ms) >= TEMP_BURST_PERIOD_MS) {
        s_last_temp_burst_ms = now;
        s_next_temp_frame = 0u;
    }
}
