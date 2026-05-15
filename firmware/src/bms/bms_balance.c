/* bms_balance.c — cell balance logic (STUB: always disabled). */
#include "bms_balance.h"
#include "ltc6812.h"
#include <string.h>

void bms_balance_tick(const CellSnapshot *cells,
                       uint64_t            active_faults,
                       BmsState            state,
                       const BmsConfig    *cfg) {
    (void)cells; (void)cfg;

    /* Disable balancing if: any blocking fault active, or state is not suitable */
    bool should_balance = (state == BMS_STATE_STANDBY || state == BMS_STATE_DISCHARGE)
                          && !(active_faults & FAULT_BLOCKS_DISCHARGE_MASK);

    if (!should_balance) {
        bms_balance_disable_all();
        return;
    }
    /* STUB: full balance algorithm not implemented */
    bms_balance_disable_all();
}

void bms_balance_disable_all(void) {
    ltc6812_cell_chain_clear_balance(BMS_CHAIN_CELL, CELL_IC_COUNT);
}
