/* bms_diagnostics.c — diagnostic counters, reset cause, and open-wire results. */
#include "bms_diagnostics.h"
#include "bms_hal.h"
#include <string.h>

static BmsDiagnostics s_diag;

void bms_diagnostics_init(void) {
    memset(&s_diag, 0, sizeof(s_diag));

    /* Capture reset cause from RCC_CSR reset flag bits before clearing them. */
    uint32_t csr = RCC->CSR;
    uint8_t cause = 0;
    if (csr & RCC_CSR_PORRSTF)   { cause |= RESET_CAUSE_POR;  }
    if (csr & RCC_CSR_PINRSTF)   { cause |= RESET_CAUSE_PIN;  }
    if (csr & RCC_CSR_SFTRSTF)   { cause |= RESET_CAUSE_SOFT; }
    if (csr & RCC_CSR_IWDGRSTF)  { cause |= RESET_CAUSE_IWDG; }
    if (csr & RCC_CSR_WWDGRSTF)  { cause |= RESET_CAUSE_WWDG; }
    if (csr & RCC_CSR_LPWRRSTF)  { cause |= RESET_CAUSE_LPWR; }
    s_diag.reset_cause = cause;

    /* Clear all reset flags so next boot only sees new flags. */
    RCC->CSR |= RCC_CSR_RMVF;
}

void bms_diagnostics_record_pec_error(BmsChain chain) {
    if (chain == BMS_CHAIN_CELL) {
        if (s_diag.pec_errors_cell < UINT32_MAX) { s_diag.pec_errors_cell++; }
    } else {
        if (s_diag.pec_errors_temp < UINT32_MAX) { s_diag.pec_errors_temp++; }
    }
}

void bms_diagnostics_record_i2c_error(void) {
    if (s_diag.i2c_errors < UINT32_MAX) { s_diag.i2c_errors++; }
}

void bms_diagnostics_set_open_wire(bool valid,
                                    const bool detected[TOTAL_CELL_COUNT]) {
    s_diag.open_wire_valid = valid;
    if (valid) {
        memcpy(s_diag.open_wire_detected, detected,
               TOTAL_CELL_COUNT * sizeof(bool));
    } else {
        memset(s_diag.open_wire_detected, 0,
               TOTAL_CELL_COUNT * sizeof(bool));
    }
}

const BmsDiagnostics *bms_diagnostics_get(void) {
    return &s_diag;
}
