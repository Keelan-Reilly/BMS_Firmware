/* bms_state.c — BMS state machine (skeleton).
 * STUB: remains in STANDBY; no discharge/charge transitions yet.
 */
#include "bms_state.h"
#include "bms_outputs.h"
#include "bms_hal.h"
#include "bms_constants.h"
#include <string.h>

static BmsState s_state;
static bool     s_charger_present;
static bool     s_bl_entry_requested;

void bms_state_init(void) {
    s_state = BMS_STATE_INIT;
    s_charger_present = false;
    s_bl_entry_requested = false;
}

void bms_state_tick(const CellSnapshot    *cells,
                     const TempSnapshot    *temps,
                     const PackMeasurement *pack,
                     uint64_t               active_faults,
                     BmsPermissionRequest  *req_out) {
    (void)cells; (void)temps; (void)pack;

    memset(req_out, 0, sizeof(*req_out));

    /* Bootloader entry overrides everything */
    if (s_bl_entry_requested) {
        bms_outputs_deassert_all();
        /* Write boot flag to RTC BKP0R */
        /* Requires PWR clock and DBP bit set — simplified for skeleton */
        RTC->BKP0R = BL_ENTRY_FLAG;
        /* Trigger system reset */
        SCB->AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
        while (1) { /* wait for reset */ }
    }

    /* Fatal fault: go to FAULT state, no permissions */
    if (active_faults & FAULT_FATAL_MASK) {
        s_state = BMS_STATE_FAULT;
        return; /* req_out all-false */
    }

    switch (s_state) {
        case BMS_STATE_INIT:
            /* Transition to STANDBY once measurements are valid */
            s_state = BMS_STATE_STANDBY;
            break;

        case BMS_STATE_STANDBY:
            /* STUB: no automatic transition to DISCHARGE/CHARGE yet */
            break;

        case BMS_STATE_FAULT:
            /* Remain in FAULT until all blocking faults clear */
            if (!active_faults) { s_state = BMS_STATE_STANDBY; }
            break;

        default:
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
