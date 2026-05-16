/* bms_state.c — BMS state machine.
 *
 * Transition summary:
 *   INIT      → STANDBY  : immediately on first tick
 *   STANDBY   → PRECHARGE: s_discharge_requested set, no discharge-blocking faults
 *   STANDBY   → CHARGE   : charger present, no charge-blocking faults
 *   PRECHARGE → DISCHARGE: Vpack within precharge_delta_max_pct of Vbat
 *   PRECHARGE → STANDBY  : timeout (sets FAULT_BIT_PRECHARGE_TIMEOUT)
 *   DISCHARGE → STANDBY  : discharge released or discharge-blocking fault clears path
 *   CHARGE    → STANDBY  : charger disconnected or charge-blocking fault
 *   any       → FAULT    : fatal fault active
 *   FAULT     → STANDBY  : all active faults clear
 */
#include "bms_state.h"
#include "bms_faults.h"
#include "bms_outputs.h"
#include "bms_config.h"
#include "board_clock.h"
#include "bms_hal.h"
#include "bms_constants.h"
#include <string.h>

static BmsState s_state;
static bool     s_charger_present;
static bool     s_discharge_requested;
static bool     s_bl_entry_requested;
static uint32_t s_precharge_start_ms;

void bms_state_init(void) {
    s_state               = BMS_STATE_INIT;
    s_charger_present     = false;
    s_discharge_requested = false;
    s_bl_entry_requested  = false;
    s_precharge_start_ms  = 0;
}

void bms_state_tick(const CellSnapshot    *cells,
                     const TempSnapshot    *temps,
                     const PackMeasurement *pack,
                     uint64_t               active_faults,
                     BmsPermissionRequest  *req_out) {
    (void)cells; (void)temps;

    memset(req_out, 0, sizeof(*req_out));

    /* Bootloader entry takes unconditional priority. */
    if (s_bl_entry_requested) {
        bms_outputs_deassert_all();
        RTC->BKP0R = BL_ENTRY_FLAG;
        SCB->AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
        while (1) { /* wait for reset */ }
    }

    /* Fatal fault forces FAULT state and drops all permissions. */
    if (active_faults & FAULT_FATAL_MASK) {
        s_state = BMS_STATE_FAULT;
        return;
    }

    const BmsConfig *cfg = bms_config_get();

    switch (s_state) {
        case BMS_STATE_INIT:
            s_state = BMS_STATE_STANDBY;
            break;

        case BMS_STATE_STANDBY:
            if (s_discharge_requested &&
                !(active_faults & FAULT_BLOCKS_DISCHARGE_MASK)) {
                s_precharge_start_ms = board_clock_get_ms();
                s_state = BMS_STATE_PRECHARGE;
            } else if (s_charger_present &&
                       !(active_faults & FAULT_BLOCKS_CHARGE_MASK)) {
                s_state = BMS_STATE_CHARGE;
            }
            break;

        case BMS_STATE_PRECHARGE: {
            uint32_t elapsed = board_clock_get_ms() - s_precharge_start_ms;

            /* Precharge timeout: set fault, return to standby. */
            if (elapsed >= cfg->precharge_timeout_ms) {
                bms_faults_set(FAULT_BIT_PRECHARGE_TIMEOUT);
                s_state = BMS_STATE_STANDBY;
                break;
            }

            /* Abort if a discharge-blocking fault appeared. */
            if (active_faults & FAULT_BLOCKS_DISCHARGE_MASK) {
                s_state = BMS_STATE_STANDBY;
                break;
            }

            /* Check Vpack vs Vbat delta once both are valid. */
            if (pack->vpack_valid && pack->vbat_valid && pack->vbat_mv > 0) {
                int32_t delta_mv = pack->vbat_mv - pack->vpack_mv;
                if (delta_mv < 0) { delta_mv = -delta_mv; }
                /* precharge_delta_max_pct is a % × 10 (per config schema). */
                int32_t threshold_mv = (pack->vbat_mv * (int32_t)cfg->precharge_delta_max_pct) / 1000;
                if (delta_mv <= threshold_mv) {
                    s_state = BMS_STATE_DISCHARGE;
                }
            }

            if (s_state == BMS_STATE_PRECHARGE) {
                /* Precharge phase: MasterOk only (enables precharge contactor). */
                req_out->want_master_ok = true;
            }
            break;
        }

        case BMS_STATE_DISCHARGE:
            if (!s_discharge_requested ||
                (active_faults & FAULT_BLOCKS_DISCHARGE_MASK)) {
                s_state = BMS_STATE_STANDBY;
                break;
            }
            req_out->want_master_ok = true;
            req_out->want_discharge = true;
            break;

        case BMS_STATE_CHARGE:
            if (!s_charger_present ||
                (active_faults & FAULT_BLOCKS_CHARGE_MASK)) {
                s_state = BMS_STATE_STANDBY;
                break;
            }
            req_out->want_master_ok      = true;
            req_out->want_charge         = true;
            req_out->want_charger_safety = true;
            break;

        case BMS_STATE_FAULT:
            if (!active_faults) { s_state = BMS_STATE_STANDBY; }
            break;

        case BMS_STATE_SHUTDOWN:
            /* No permissions; wait for power-down. */
            break;
    }
}

BmsState bms_state_get(void) { return s_state; }

void bms_state_request_bootloader_entry(void) {
    s_bl_entry_requested = true;
}

void bms_state_notify_charger_present(bool present) {
    s_charger_present = present;
}

void bms_state_set_discharge_requested(bool requested) {
    s_discharge_requested = requested;
}
