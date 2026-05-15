/* bms_types.h — shared enums, structs, and measurement types used across
 * BMS application modules. Does not include hardware register definitions.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "bms_constants.h"

/* ── BMS state machine ────────────────────────────────────────────────────── */
typedef enum {
    BMS_STATE_INIT          = 0,   /* startup, hardware not yet validated */
    BMS_STATE_STANDBY       = 1,   /* idle, no permissions granted */
    BMS_STATE_PRECHARGE     = 2,   /* precharge contactor sequence */
    BMS_STATE_DISCHARGE     = 3,   /* discharge path active */
    BMS_STATE_CHARGE        = 4,   /* charge path active */
    BMS_STATE_FAULT         = 5,   /* fatal or blocking fault present */
    BMS_STATE_SHUTDOWN      = 6,   /* power-down sequence */
} BmsState;

/* ── Chain identifiers ────────────────────────────────────────────────────── */
typedef enum {
    BMS_CHAIN_CELL = 0,   /* LTC6812 cell measurement chain, CS = PA4  */
    BMS_CHAIN_TEMP = 1,   /* LTC6812 temperature chain,     CS = PB12 */
} BmsChain;

/* ── Result codes (module-level) ──────────────────────────────────────────── */
typedef enum {
    BMS_OK                  = 0,
    BMS_ERR_TIMEOUT         = 1,
    BMS_ERR_PEC             = 2,   /* PEC-15 mismatch from LTC6812 */
    BMS_ERR_SPI             = 3,
    BMS_ERR_I2C             = 4,
    BMS_ERR_INVALID_ARG     = 5,
    BMS_ERR_FORBIDDEN       = 6,   /* operation not permitted on this chain */
    BMS_ERR_CONFIG_INVALID  = 7,
    BMS_ERR_FLASH           = 8,
    BMS_ERR_NOT_SUPPORTED   = 9,
    BMS_ERR_STALE           = 10,  /* measurement data is stale */
} BmsResult;

/* ── Measurement validity ─────────────────────────────────────────────────── */
typedef enum {
    MEAS_VALID   = 0,
    MEAS_INVALID = 1,   /* not yet acquired or stale */
    MEAS_ERROR   = 2,   /* acquisition error (PEC, SPI, etc.) */
} MeasValidity;

/* ── Cell voltage snapshot ────────────────────────────────────────────────── */
typedef struct {
    uint16_t     mv[TOTAL_CELL_COUNT]; /* raw cell voltages in mV */
    bool         valid[TOTAL_CELL_COUNT];
    uint32_t     timestamp_ms;
    MeasValidity overall;
} CellSnapshot;

/* ── Temperature snapshot ─────────────────────────────────────────────────── */
/* Temperatures stored as °C × 10 (int16_t). 0x8000 = INVALID sentinel. */
#define TEMP_INVALID_CX10  ((int16_t)0x8000)

typedef struct {
    int16_t      cx10[TOTAL_TEMP_COUNT]; /* °C × 10 or TEMP_INVALID_CX10 */
    bool         valid[TOTAL_TEMP_COUNT];
    uint32_t     timestamp_ms;
    MeasValidity overall;
} TempSnapshot;

/* ── Pack-level measurements ──────────────────────────────────────────────── */
typedef struct {
    int32_t      vbat_mv;       /* battery terminal voltage via ISL28022; INT32_MIN if invalid */
    int32_t      vpack_mv;      /* load-side voltage via PA1 ADC;         INT32_MIN if invalid */
    int32_t      i_batt_ma;     /* positive = discharge */
    bool         vbat_valid;
    bool         vpack_valid;
    bool         i_batt_valid;
    uint32_t     timestamp_ms;
} PackMeasurement;

/* ── Fault bitmaps ────────────────────────────────────────────────────────── */
/* Bit positions — see protocol/fault_bits.yaml for full definitions. */
typedef enum {
    FAULT_BIT_CELL_UV_HARD        = 0,
    FAULT_BIT_CELL_UV_SOFT        = 1,
    FAULT_BIT_CELL_OV_HARD        = 2,
    FAULT_BIT_CELL_OV_SOFT        = 3,
    FAULT_BIT_CELL_MEASUREMENT    = 4,
    FAULT_BIT_CELL_OPEN_WIRE      = 5,
    FAULT_BIT_TEMP_HIGH_CHARGE    = 6,
    FAULT_BIT_TEMP_HIGH_DISCHARGE = 7,
    FAULT_BIT_TEMP_COLD_CHARGE    = 8,
    FAULT_BIT_TEMP_COLD_DISCHARGE = 9,
    FAULT_BIT_TEMP_MEASUREMENT    = 10,
    FAULT_BIT_OVERCURRENT_HARD    = 11,
    FAULT_BIT_OVERCURRENT_WARN    = 12,
    FAULT_BIT_VBAT_INVALID        = 13,
    FAULT_BIT_VPACK_INVALID       = 14,
    FAULT_BIT_PRECHARGE_TIMEOUT   = 15,
    FAULT_BIT_PRECHARGE_DELTA     = 16,
    FAULT_BIT_CONFIG_INVALID      = 17,
    FAULT_BIT_STALE_CELLS         = 18,
    FAULT_BIT_STALE_TEMPS         = 19,
    FAULT_BIT_STALE_PACK          = 20,
    FAULT_BIT_PEC_ERROR           = 21,
    FAULT_BIT_I2C_ERROR           = 22,
    FAULT_BIT_INTERNAL            = 23,
    FAULT_BIT_WATCHDOG            = 24,
    /* bits 25–63 reserved */
} FaultBit;

#define FAULT_MASK(bit)   ((uint64_t)1u << (bit))

/* Severity threshold for deassert-all / halt behaviour */
#define FAULT_FATAL_MASK  (FAULT_MASK(FAULT_BIT_CONFIG_INVALID)  | \
                           FAULT_MASK(FAULT_BIT_INTERNAL)         | \
                           FAULT_MASK(FAULT_BIT_WATCHDOG))

#define FAULT_BLOCKS_DISCHARGE_MASK \
    (FAULT_MASK(FAULT_BIT_CELL_UV_HARD)        | \
     FAULT_MASK(FAULT_BIT_CELL_OV_HARD)        | \
     FAULT_MASK(FAULT_BIT_CELL_MEASUREMENT)    | \
     FAULT_MASK(FAULT_BIT_TEMP_HIGH_DISCHARGE) | \
     FAULT_MASK(FAULT_BIT_TEMP_COLD_DISCHARGE) | \
     FAULT_MASK(FAULT_BIT_TEMP_MEASUREMENT)    | \
     FAULT_MASK(FAULT_BIT_OVERCURRENT_HARD)    | \
     FAULT_MASK(FAULT_BIT_VBAT_INVALID)        | \
     FAULT_MASK(FAULT_BIT_VPACK_INVALID)       | \
     FAULT_MASK(FAULT_BIT_STALE_CELLS)         | \
     FAULT_MASK(FAULT_BIT_STALE_PACK)          | \
     FAULT_MASK(FAULT_BIT_CONFIG_INVALID))

#define FAULT_BLOCKS_CHARGE_MASK \
    (FAULT_MASK(FAULT_BIT_CELL_OV_HARD)        | \
     FAULT_MASK(FAULT_BIT_CELL_MEASUREMENT)    | \
     FAULT_MASK(FAULT_BIT_TEMP_HIGH_CHARGE)    | \
     FAULT_MASK(FAULT_BIT_TEMP_COLD_CHARGE)    | \
     FAULT_MASK(FAULT_BIT_TEMP_MEASUREMENT)    | \
     FAULT_MASK(FAULT_BIT_VBAT_INVALID)        | \
     FAULT_MASK(FAULT_BIT_STALE_CELLS)         | \
     FAULT_MASK(FAULT_BIT_STALE_PACK)          | \
     FAULT_MASK(FAULT_BIT_CONFIG_INVALID))

/* ── Permission request struct (passed to bms_outputs) ───────────────────── */
typedef struct {
    bool want_master_ok;
    bool want_discharge;
    bool want_charge;
    bool want_charger_safety;
} BmsPermissionRequest;

/* ── Output state bitmask (for protocol reporting) ───────────────────────── */
/* bit0=MasterOk, bit1=Discharge, bit2=Charge, bit3=ChargerSafety */
typedef uint8_t BmsOutputsBitmask;
#define OUTPUTS_BIT_MASTER_OK       (0x01u)
#define OUTPUTS_BIT_DISCHARGE       (0x02u)
#define OUTPUTS_BIT_CHARGE          (0x04u)
#define OUTPUTS_BIT_CHARGER_SAFETY  (0x08u)

/* ── Protocol error codes ─────────────────────────────────────────────────── */
typedef enum {
    PROTO_OK                    = 0x00,
    PROTO_ERR_UNKNOWN_PACKET    = 0x01,
    PROTO_ERR_BAD_LENGTH        = 0x02,
    PROTO_ERR_BAD_CRC           = 0x03,
    PROTO_ERR_BAD_SEQUENCE      = 0x04,
    PROTO_ERR_CONFIG_INVALID    = 0x05,
    PROTO_ERR_WRONG_TARGET      = 0x06,
    PROTO_ERR_PACKAGE_INVALID   = 0x07,
    PROTO_ERR_FLASH_FAIL        = 0x08,
    PROTO_ERR_BUSY              = 0x09,
    PROTO_ERR_NOT_SUPPORTED     = 0x0A,
    PROTO_ERR_BAD_STATE         = 0x0B,
    PROTO_ERR_VERSION_MISMATCH  = 0x0C,
    PROTO_ERR_INTERNAL          = 0x0D,
} ProtoError;
