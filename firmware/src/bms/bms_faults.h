/* bms_faults.h — fault evaluation and latching. */
#pragma once
#include <stdint.h>
#include "bms_types.h"
#include "bms_config.h"

/* Evaluate all fault conditions from current measurements.
 * Updates internal active and latched fault bitmaps.
 * Must be called after every measurement cycle. */
void bms_faults_evaluate(const CellSnapshot    *cells,
                          const TempSnapshot    *temps,
                          const PackMeasurement *pack,
                          const BmsConfig       *cfg);

/* Returns current active fault bitmap (conditions present now). */
uint64_t bms_faults_get_active(void);

/* Returns latched fault bitmap (sticky until explicitly cleared). */
uint64_t bms_faults_get_latched(void);

/* Attempt to clear latched faults in mask. Only clears faults whose active
 * condition has resolved. Returns bitmask of faults actually cleared. */
uint64_t bms_faults_clear_latched(uint64_t mask);

/* Set a specific fault bit directly (for internal error reporting). */
void bms_faults_set(FaultBit bit);
