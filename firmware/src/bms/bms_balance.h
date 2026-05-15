/* bms_balance.h — cell balance management. */
#pragma once
#include "bms_types.h"
#include "bms_config.h"

/* Evaluate which cells should be balanced based on measurements and config.
 * Only operates on CELL chain via ltc6812_cell_chain_set_balance().
 * Balancing is disabled (DCC=0) if: any fault in FAULT_BLOCKS_DISCHARGE_MASK,
 * state is not DISCHARGE, or any cell is outside safe voltage window. */
void bms_balance_tick(const CellSnapshot *cells,
                       uint64_t            active_faults,
                       BmsState            state,
                       const BmsConfig    *cfg);

/* Immediately disable all balancing (DCC bits cleared on CELL chain). */
void bms_balance_disable_all(void);
