/* bms_diagnostics.h — diagnostic counters, reset cause, open-wire results,
 *                      and bring-up probe result storage. */
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

/* ── Bring-up probe results ───────────────────────────────────────────────── */

/* Per-IC probe status: one entry per IC in the chain. */
typedef struct {
    bool     responded;       /* true if IC replied (PEC valid on RDCFGA) */
    uint8_t  cfga[6];         /* raw CFGA register bytes (little-endian) */
} BmsIcProbeStatus;

/* Result of a chain probe (CELL or TEMP). */
typedef struct {
    bool              run;              /* true if a probe has been executed */
    BmsResult         result;           /* overall result (BMS_OK or error code) */
    uint8_t           ic_count;         /* number of ICs expected */
    BmsIcProbeStatus  ic[5];            /* per-IC (max CELL_IC_COUNT/TEMP_IC_COUNT) */
    uint32_t          timestamp_ms;
} BmsChainProbeResult;

/* Result of ISL28022 I2C probe. */
typedef struct {
    bool      run;
    BmsResult result;          /* BMS_OK = ACK received */
    uint16_t  config_reg;      /* raw config register value (0xFFFF if failed) */
    uint32_t  timestamp_ms;
} BmsIslProbeResult;

/* Vpack ADC raw read result. */
typedef struct {
    bool      run;
    BmsResult result;
    uint16_t  raw_code;        /* 12-bit ADC result (0–4095) */
    uint32_t  timestamp_ms;
} BmsVpackRawResult;

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

    /* Bring-up probe results (populated on demand by protocol handlers) */
    BmsChainProbeResult cell_probe;
    BmsChainProbeResult temp_probe;
    BmsIslProbeResult   isl_probe;
    BmsVpackRawResult   vpack_raw;
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

/* Store bring-up probe results. */
void bms_diagnostics_store_cell_probe(const BmsChainProbeResult *r);
void bms_diagnostics_store_temp_probe(const BmsChainProbeResult *r);
void bms_diagnostics_store_isl_probe(const BmsIslProbeResult *r);
void bms_diagnostics_store_vpack_raw(const BmsVpackRawResult *r);
