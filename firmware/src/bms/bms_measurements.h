/* bms_measurements.h — periodic measurement acquisition. */
#pragma once
#include "bms_types.h"
#include <stdint.h>
#include <stdbool.h>

/* Run one cell measurement cycle on the CELL chain.
 * Updates the internal cell snapshot; callers read via bms_measurements_get_cells(). */
BmsResult bms_measurements_run_cell_cycle(void);

/* Run one temperature measurement cycle on the TEMP chain.
 * Asserts S-outputs, waits settle time, runs ADAX, clears S-outputs.
 * MUST clear S-outputs in both success and error paths (enforced here). */
BmsResult bms_measurements_run_temp_cycle(void);

/* Run one pack measurement cycle (ISL28022 Vbat/I + PA1 Vpack ADC). */
BmsResult bms_measurements_run_pack_cycle(void);

/* Accessors — return pointers to internal snapshots; data is stable until
 * next run_*_cycle() call. */
const CellSnapshot    *bms_measurements_get_cells(void);
const TempSnapshot    *bms_measurements_get_temps(void);
const PackMeasurement *bms_measurements_get_pack(void);

/* Update pack measurement from ISL28022/ADC results.
 * Called by the ISL28022 driver and ADC driver on successful reads. */
void bms_measurements_update_pack(int32_t vbat_mv, int32_t vpack_mv,
                                   int32_t i_batt_ma,
                                   bool vbat_valid, bool vpack_valid,
                                   bool i_batt_valid);
