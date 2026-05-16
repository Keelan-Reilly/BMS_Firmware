/* bms_diagnostics.h — diagnostic counters, reset cause, and open-wire results. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "bms_types.h"

/* Reset cause flags (from RCC_CSR reset flags register) */
#define RESET_CAUSE_POR     (1u << 0)   /* Power-on reset */
#define RESET_CAUSE_PIN     (1u << 1)   /* NRST pin reset */
#define RESET_CAUSE_SOFT    (1u << 2)   /* Software reset (SYSRESETREQ) */
#define RESET_CAUSE_IWDG    (1u << 3)   /* IWDG watchdog */
#define RESET_CAUSE_WWDG    (1u << 4)   /* WWDG window watchdog */
#define RESET_CAUSE_LPWR    (1u << 5)   /* Low-power reset */

typedef struct {
    /* Reset cause at boot (bitmask of RESET_CAUSE_* flags) */
    uint8_t  reset_cause;

    /* PEC error counters (cumulative since boot) */
    uint32_t pec_errors_cell;
    uint32_t pec_errors_temp;

    /* I2C error counter (cumulative since boot) */
    uint32_t i2c_errors;

    /* Open-wire diagnostic results (populated by ltc6812_run_open_wire) */
    bool     open_wire_valid;             /* true if last run completed without error */
    bool     open_wire_detected[TOTAL_CELL_COUNT];

    /* Uptime (provided by caller from board_clock_get_ms) */
    uint32_t uptime_ms;
} BmsDiagnostics;

/* Initialise diagnostics: capture reset cause from RCC registers and clear counters. */
void bms_diagnostics_init(void);

/* Record a PEC error on the given chain (increments cumulative counter). */
void bms_diagnostics_record_pec_error(BmsChain chain);

/* Record an I2C error. */
void bms_diagnostics_record_i2c_error(void);

/* Store open-wire results after a completed diagnostic run. */
void bms_diagnostics_set_open_wire(bool valid,
                                    const bool detected[TOTAL_CELL_COUNT]);

/* Returns pointer to current diagnostics snapshot. */
const BmsDiagnostics *bms_diagnostics_get(void);
