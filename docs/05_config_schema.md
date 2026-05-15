# 05 — Configuration Schema

## 1. Overview

The config blob is a fixed-length, little-endian packed struct stored in flash (two redundant slots: Config A and Config B). It is versioned, length-checked, CRC32-protected, and hardware-profile-checked. The firmware rejects any config blob that fails any check.

All fields are stored in little-endian byte order. No padding bytes unless explicitly listed as reserved.

---

## 2. Schema

### Header (64 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 0 | 4 | magic | uint32_t | 0xBBCC0001 | Exactly 0xBBCC0001 | Must match exactly |
| 4 | 2 | schema_version | uint16_t | 1 | 1 | Must match `CONFIG_SCHEMA_VERSION` |
| 6 | 2 | total_length | uint16_t | (see §3) | (see §3) | Must match compile-time schema size |
| 8 | 2 | hw_profile_id | uint16_t | (TBD) | Exactly HW_PROFILE_ID | Must match firmware's `HW_PROFILE_ID` |
| 10 | 4 | config_generation | uint32_t | 0 | 0–0xFFFFFFFE | Increment on each store; 0xFFFFFFFF reserved |
| 14 | 4 | config_crc32 | uint32_t | (computed) | — | CRC32 of bytes 0..total_length-5 (excludes last 4 bytes if CRC is last — see §5) |
| 18 | 46 | reserved_header | uint8_t[46] | 0x00 | All zero | Must be zero (reserved for future extension) |

Total header: 64 bytes.

---

### Cell Topology (4 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 64 | 1 | cell_count | uint8_t | 75 | 1–75 | Must equal 75 for this hardware profile |
| 65 | 1 | temp_count | uint8_t | 75 | 1–75 | Must equal 75 for this hardware profile |
| 66 | 2 | reserved_topology | uint16_t | 0 | — | Must be zero |

---

### Cell Voltage Thresholds (16 bytes)

All values in mV (uint16_t).

| Offset | Size | Field | Type | Default (mV) | Valid Range (mV) | Validation Rule |
|---|---|---|---|---|---|---|
| 68 | 2 | cell_uv_soft_mv | uint16_t | 3000 | 1000–5000 | Must be < cell_uv_hard_mv |
| 70 | 2 | cell_uv_hard_mv | uint16_t | 2750 | 1000–5000 | Must be < cell_uv_soft_mv; < cell_ov_hard_mv |
| 72 | 2 | cell_ov_soft_mv | uint16_t | 4150 | 1000–5000 | Must be > cell_ov_hard_mv; > cell_uv_soft_mv |
| 74 | 2 | cell_ov_hard_mv | uint16_t | 4200 | 1000–5000 | Must be > cell_ov_soft_mv |
| 76 | 2 | cell_balance_target_mv | uint16_t | 3800 | 1000–5000 | Must be ≥ cell_uv_hard_mv; ≤ cell_ov_soft_mv |
| 78 | 2 | cell_balance_hysteresis_mv | uint16_t | 10 | 0–500 | Must be < (cell_ov_soft_mv - cell_balance_target_mv) |
| 80 | 2 | cell_nominal_mv | uint16_t | 3700 | 1000–5000 | Informational; used for SOC estimation (future) |
| 82 | 2 | reserved_cell_thresholds | uint16_t | 0 | — | Must be zero |

**Threshold ordering invariant:** `cell_uv_hard_mv < cell_uv_soft_mv < cell_balance_target_mv < cell_ov_soft_mv < cell_ov_hard_mv`

---

### Temperature Thresholds (16 bytes)

All values in °C × 10 (int16_t). e.g., 450 = 45.0°C, -100 = -10.0°C.

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 84 | 2 | temp_charge_warn_cx10 | int16_t | 400 | -500–1000 | < temp_charge_hard_cx10 |
| 86 | 2 | temp_charge_hard_cx10 | int16_t | 450 | -500–1000 | > temp_charge_warn_cx10; ≤ temp_hard_abs_cx10 |
| 88 | 2 | temp_discharge_warn_cx10 | int16_t | 550 | -500–1000 | < temp_discharge_hard_cx10 |
| 90 | 2 | temp_discharge_hard_cx10 | int16_t | 600 | -500–1000 | > temp_discharge_warn_cx10; ≤ temp_hard_abs_cx10 |
| 92 | 2 | temp_hard_abs_cx10 | int16_t | 700 | -500–1000 | ≥ max(temp_charge_hard_cx10, temp_discharge_hard_cx10) |
| 94 | 2 | temp_cold_charge_limit_cx10 | int16_t | 0 | -500–500 | < 100; below this temperature, charging is prohibited |
| 96 | 2 | temp_cold_discharge_limit_cx10 | int16_t | -200 | -500–500 | ≤ temp_cold_charge_limit_cx10 |
| 98 | 2 | reserved_temp_thresholds | uint16_t | 0 | — | Must be zero |

> **OPEN QUESTION:** Confirm Enepaq/Sony-Murata cell absolute temperature limits from datasheet and set defaults accordingly.

---

### Current Limits (8 bytes)

Values in mA (uint32_t).

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 100 | 4 | overcurrent_hard_ma | uint32_t | 100000 | 1000–500000 | > 0 |
| 104 | 4 | overcurrent_warn_ma | uint32_t | 80000 | 1000–500000 | ≤ overcurrent_hard_ma |

---

### Precharge Parameters (8 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 108 | 2 | precharge_pct | uint16_t | 90 | 50–99 | % of Vbat that Vpack must reach |
| 110 | 4 | precharge_timeout_ms | uint32_t | 10000 | 500–60000 | > 0 |
| 114 | 2 | precharge_delta_max_pct | uint16_t | 5 | 1–20 | Max |Vpack-Vbat|/Vbat × 100 after completion |

---

### Balancing Parameters (8 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 116 | 4 | balance_on_time_ms | uint32_t | 5000 | 100–30000 | Duty cycle on-time per balance period |
| 120 | 4 | balance_off_time_ms | uint32_t | 1000 | 100–30000 | Duty cycle off-time per balance period |

---

### Temperature Measurement Parameters (4 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 124 | 2 | temp_settle_time_ms | uint16_t | 5 | 1–100 | Sensor bias settle time before conversion |
| 126 | 2 | reserved_temp_params | uint16_t | 0 | — | Must be zero |

---

### Stale Data Timeout (4 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Validation Rule |
|---|---|---|---|---|---|---|
| 128 | 4 | stale_data_timeout_ms | uint32_t | 500 | 100–5000 | > 0 |

---

### Cell Masks (30 bytes each, 75 bits used)

Stored as 10-byte (80-bit) arrays, little-endian bit order. Bit N corresponds to cell/sensor index N (0-based). Bits 75–79 must be zero. Firmware rejects any mask with bits 75–79 set.

| Offset | Size | Field | Type | Default | Validation Rule |
|---|---|---|---|---|---|
| 132 | 10 | required_cell_mask | uint8_t[10] | all 1s | Bits 75–79 must be 0 |
| 142 | 10 | required_temp_mask | uint8_t[10] | all 1s | Bits 75–79 must be 0 |
| 152 | 10 | balance_allowed_mask | uint8_t[10] | all 1s | Bits 75–79 must be 0 |

---

### Calibration (16 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Notes |
|---|---|---|---|---|---|---|
| 162 | 4 | vpack_gain_x1000 | uint32_t | 1000 | 100–10000 | Divider ratio × 1000; Vpack = raw × VREF × gain / (4095 × 1000) |
| 166 | 4 | vpack_offset_mv | int32_t | 0 | -5000–5000 | Added after gain scaling |
| 170 | 2 | vbat_gain_x1000 | uint16_t | 1000 | 100–10000 | ISL28022 bus voltage scale factor |
| 172 | 2 | vbat_offset_mv | int16_t | 0 | -5000–5000 | |
| 174 | 2 | current_gain_x1000 | uint16_t | 1000 | 100–10000 | Shunt current scale factor |
| 176 | 2 | current_offset_ma | int16_t | 0 | -1000–1000 | |

---

### CAN / Communication Options (8 bytes)

| Offset | Size | Field | Type | Default | Valid Range | Notes |
|---|---|---|---|---|---|---|
| 178 | 4 | can_watchdog_timeout_ms | uint32_t | 0 | 0–60000 | 0 = disabled |
| 182 | 2 | can_base_id | uint16_t | 0x500 | 0x000–0x7FF | Base CAN message ID |
| 184 | 2 | reserved_can | uint16_t | 0 | — | Must be zero |

---

### Reserved (40 bytes)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 186 | 40 | reserved | All zero; for future fields |

---

## 3. Total Schema Size

**Total: 226 bytes**

| Section | Bytes |
|---|---|
| Header | 64 |
| Topology | 4 |
| Cell thresholds | 16 |
| Temp thresholds | 16 |
| Current limits | 8 |
| Precharge | 8 |
| Balancing | 8 |
| Temp params | 4 |
| Stale timeout | 4 |
| Masks × 3 | 30 |
| Calibration | 16 |
| CAN/comms | 8 |
| Reserved | 40 |
| **Total** | **226** |

The `total_length` field must equal 226 for schema version 1. Firmware rejects any other value.

---

## 4. CRC32 Coverage

`config_crc32` (at offset 14) is computed over bytes `[0..221]` (all bytes except the 4 CRC bytes themselves). The CRC bytes are at offsets 14–17. For CRC computation, treat offsets 14–17 as zero.

CRC algorithm: CRC-32/ISO-HDLC (standard Ethernet polynomial 0xEDB88320, reflected, init=0xFFFFFFFF, final XOR=0xFFFFFFFF).

---

## 5. Validation Order

The firmware validates in this order (fail-fast):

1. `magic` == 0xBBCC0001
2. `total_length` == CONFIG_SCHEMA_SIZE (226)
3. `schema_version` == CONFIG_SCHEMA_VERSION
4. `hw_profile_id` == HW_PROFILE_ID
5. CRC32 computed and compared to `config_crc32`
6. `cell_count` == CELL_COUNT (75)
7. `temp_count` == TEMP_COUNT (75)
8. Threshold ordering: `cell_uv_hard < cell_uv_soft < balance_target < cell_ov_soft < cell_ov_hard`
9. Temperature threshold ordering (as specified per field)
10. All mask high bits (75–79) == 0
11. Reserved fields all == 0

Any step failure: return specific error code indicating which check failed.

---

## 6. Bad Config Fallback Policy

If stored config fails validation at boot:
- `FAULT_CONFIG_INVALID` is set (latched)
- Firmware loads safe defaults (all thresholds maximally conservative; all masks = 0; all permissions blocked)
- Device enters `STATE_CONFIG_ERROR`
- Tool can connect, read diagnostics, and write a valid config
- Config is not applied until a fresh valid config is received via `STORE_CONFIG`

---

## 7. Safe Defaults Policy

Safe defaults are compile-time constants in `bms_config.c`. They are **not** written to flash. They are used only as the active config until a valid stored config is loaded or written.

Safe defaults make the BMS conservatively restrict all operations:
- All required masks = 0x00 (no cells/sensors required; permissions blocked because no valid measurement exists)
- All permissions gated by `FAULT_CONFIG_INVALID`
- No balancing

---

## 8. Version Migration Policy

For v1: no migration. If `schema_version` does not match, firmware rejects the config. A new config must be written via the tool.

For future versions: add a migration function `bms_config_migrate(old_version, old_blob) -> new_blob` that is explicitly invoked during `bms_config_load()` if `schema_version < CONFIG_SCHEMA_VERSION`. Migration must be documented with the new schema version change.

---

## 9. Storage: Dual-Slot Design

Two config slots in flash (Config A and Config B; see flash map in `docs/06_flash_and_bootloader.md`):

- On load: read both slots; select the one with higher `config_generation` among valid configs
- If both valid: use higher generation
- If one valid: use it; mark the other slot as needing sync
- If neither valid: use safe defaults; set `FAULT_CONFIG_INVALID`
- On store: write to the slot with lower generation (or the invalid slot); validate readback; only then update the other slot on next write

This provides single-fault-tolerant config storage against a write-interrupted power loss.

---

## 10. Alignment / Packing Rules

- Struct is `__attribute__((packed))` in C (no compiler padding)
- All multi-byte fields are explicitly little-endian
- The tool (Python/Rust/etc.) must serialize with matching endianness
- The `protocol/config_schema.yaml` machine-readable file is the single source of truth for field offsets; firmware and tool both generated/validated against it
