/* bms_state.h — BMS state machine. */
#pragma once
#include "bms_types.h"

/* Initialise state machine to BMS_STATE_INIT. */
void bms_state_init(void);

/* Run one state machine tick. Must be called every main loop iteration.
 * Evaluates transitions based on current measurements, faults, and inputs.
 * Produces a permission request for bms_outputs_apply(). */
void bms_state_tick(const CellSnapshot    *cells,
                     const TempSnapshot    *temps,
                     const PackMeasurement *pack,
                     uint64_t               active_faults,
                     BmsPermissionRequest  *req_out);

/* Returns current state. */
BmsState bms_state_get(void);

/* External event: request bootloader entry (clears permissions, sets boot flag). */
void bms_state_request_bootloader_entry(void);

/* External event: charger detected (from ChargeDetect GPIO). */
void bms_state_notify_charger_present(bool present);
