# 04 — Protocol Contract

## 1. Transport Assumptions

| Parameter | Value |
|---|---|
| Physical layer | USART2 via CP2104 USB-UART bridge |
| Baud rate | 115200 (default); 921600 optional for bulk data |
| Data bits / parity / stop | 8N1 |
| Flow control | None (software flow control only if needed) |
| Byte order | Little-endian throughout |
| Direction | Request/response; host initiates all transactions |
| Max in-flight requests | 1 (no pipelining in v1) |

CAN is a separate telemetry channel — it uses different message IDs and a different framing. This protocol document covers the USB/UART desktop tool protocol only.

---

## 2. Frame Format

```
 Offset  Size  Field
 0       2     SOF: 0xAA 0x55
 2       2     PKT_ID (uint16_t LE)
 4       1     FLAGS (uint8_t)
 5       1     SEQ_ID (uint8_t): request sequence number; response echoes it
 6       2     PAYLOAD_LEN (uint16_t LE): number of payload bytes following
 8       N     PAYLOAD (N bytes)
 8+N     2     FRAME_CRC (CRC-16/CCITT, covers bytes 0..8+N-1)
```

**Minimum frame size:** 10 bytes (no payload)  
**Maximum payload size:** 1024 bytes (v1)  
**FRAME_CRC:** CRC-16/CCITT-FALSE (init=0xFFFF, poly=0x1021, no reflect)  
**CRC coverage:** All bytes from SOF through the last payload byte inclusive.

### FLAGS byte

| Bit | Meaning |
|---|---|
| 7 | `IS_RESPONSE`: 0 = request from host, 1 = response from device |
| 6 | `IS_ERROR`: 1 = this is an error response; payload contains error code |
| 5–0 | Reserved, must be 0 |

---

## 3. Request / Response Model

1. Host sends a request frame (FLAGS.IS_RESPONSE = 0).
2. Device responds with exactly one response frame echoing the same PKT_ID and SEQ_ID (FLAGS.IS_RESPONSE = 1).
3. If an error occurred, FLAGS.IS_ERROR = 1 and the payload is a 1-byte error code.
4. Host timeout: 500 ms per request; up to 3 retries with new SEQ_ID on timeout.
5. Device ignores duplicate SEQ_IDs for idempotent reads; non-idempotent writes (`STORE_CONFIG`, `BOOT_UPDATE_*`) are tracked to prevent double-execution.

---

## 4. Error Response Format

When FLAGS.IS_ERROR = 1:

```
 Offset  Size  Field
 0       1     ERROR_CODE (uint8_t)
 1       0-N   Optional error detail (string or context bytes)
```

### Error Codes

| Code | Name | Meaning |
|---|---|---|
| 0x00 | ERR_OK | No error (never sent as error response) |
| 0x01 | ERR_UNKNOWN_PACKET | PKT_ID not recognized |
| 0x02 | ERR_BAD_LENGTH | PAYLOAD_LEN wrong for this packet |
| 0x03 | ERR_CRC_FAIL | FRAME_CRC did not match |
| 0x04 | ERR_NOT_READY | Device not ready (e.g., conversion in progress) |
| 0x05 | ERR_CONFIG_INVALID | Config validation failed (see detail bytes) |
| 0x06 | ERR_WRONG_STATE | Command not valid in current state |
| 0x07 | ERR_WRONG_TARGET | Hardware profile or MCU ID mismatch |
| 0x08 | ERR_FLASH_FAIL | Flash write/erase failed |
| 0x09 | ERR_PACKAGE_INVALID | Firmware package validation failed |
| 0x0A | ERR_NOT_BOOTLOADER | Command requires bootloader mode |
| 0x0B | ERR_NO_CAPABILITIES | Capabilities not yet exchanged |
| 0x0C | ERR_ACCESS_DENIED | Operation not permitted in current mode |
| 0x0D | ERR_SEQUENCE | Chunk sequence error (bootloader) |
| 0xFF | ERR_INTERNAL | Internal firmware error |

---

## 5. Packet ID Ranges

| Range | Owner |
|---|---|
| 0x0001 – 0x00FF | System / capabilities |
| 0x0100 – 0x01FF | Monitoring / telemetry |
| 0x0200 – 0x02FF | Configuration |
| 0x0300 – 0x03FF | Diagnostics |
| 0x0400 – 0x04FF | Bootloader / firmware update |
| 0xFF00 – 0xFFFF | Reserved / test |

---

## 6. Packet Definitions

---

### `GET_CAPABILITIES` — 0x0001

**Direction:** Host → Device  
**Purpose:** First packet sent by tool; response establishes device identity and capabilities.  
**Rule:** Tool must send this before any config write or firmware update command.

**Request payload:** 0 bytes

**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | firmware_type | 0x0001 = BMS application, 0x0002 = bootloader |
| 2 | 3 | firmware_version | [major, minor, patch] |
| 5 | 2 | hw_profile_id | Hardware profile; tool must match this against package |
| 7 | 2 | protocol_version | Must be ≥ tool's minimum supported |
| 9 | 2 | config_schema_version | Current schema version on device |
| 11 | 1 | cell_count | 75 |
| 12 | 1 | temp_count | 75 |
| 13 | 4 | feature_flags | Bitfield: see below |
| 17 | 1 | max_payload_bytes_log2 | log2(max payload); 10 = 1024 bytes |
| 18 | 4 | flash_app_size | Application image size in bytes |
| 22 | 4 | flash_config_size | Config region size in bytes |

**Feature flags:**

| Bit | Name | Meaning |
|---|---|---|
| 0 | FEAT_BALANCING | Passive balancing supported |
| 1 | FEAT_OPENWIRE | Open-wire diagnostic supported |
| 2 | FEAT_BOOTLOADER | Bootloader-based update supported |
| 3 | FEAT_CAN | CAN interface present |
| 4–31 | Reserved | 0 |

**Errors:** ERR_INTERNAL

---

### `GET_VALUES` — 0x0101

**Direction:** Host → Device  
**Purpose:** Fast summary of pack state (Vbat, Vpack, current, state, fault summary).

**Request payload:** 0 bytes

**Response payload:**

| Offset | Size | Field | Units | Notes |
|---|---|---|---|---|
| 0 | 4 | vbat_mv | mV | Battery side voltage |
| 4 | 4 | vpack_mv | mV | Load side voltage |
| 8 | 4 | i_batt_ma | mA | Positive = discharging |
| 12 | 2 | state | enum BmsState | See state enum |
| 14 | 8 | active_faults | bitfield | Bit per fault (see fault_bits.yaml) |
| 22 | 8 | latched_faults | bitfield | |
| 30 | 1 | outputs_state | bitfield | Bit0=MasterOk, Bit1=DischPerm, Bit2=ChgPerm, Bit3=ChgrSafety |
| 31 | 4 | uptime_ms | ms | |
| 35 | 1 | measurement_flags | bitfield | Bit0=cells_valid, Bit1=temps_valid, Bit2=vbat_valid, Bit3=vpack_valid |

**Errors:** ERR_INTERNAL

---

### `GET_CELLS` — 0x0102

**Direction:** Host → Device  
**Purpose:** All 75 cell voltages in one response.

**Request payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | flags | Bit0: 1 = include per-cell validity flags |

**Response payload:**

| Offset | Size | Field | Units | Notes |
|---|---|---|---|---|
| 0 | 2 | cell_count | — | Always 75 |
| 2 | 2×75=150 | cell_mv[75] | mV (uint16_t) | Cells 0–74; 0 if invalid and flags requested |
| 152 | 4 | timestamp_ms | ms | Age of this measurement |
| 156 | 10 | validity_bits | bitfield | Bit per cell (75 bits); only present if flags.Bit0=1 |

**Length:** 156 bytes (no validity) or 166 bytes (with validity)  
**Errors:** ERR_NOT_READY, ERR_INTERNAL

---

### `GET_TEMPS` — 0x0103

**Direction:** Host → Device  
**Purpose:** All 75 temperature sensor readings.

**Request payload:** 0 bytes

**Response payload:**

| Offset | Size | Field | Units | Notes |
|---|---|---|---|---|
| 0 | 1 | temp_count | — | Always 75 |
| 1 | 2×75=150 | temp_cx10[75] | °C × 10 (int16_t) | Signed; 250 = 25.0°C; 0x8000 = INVALID |
| 151 | 4 | timestamp_ms | ms | |
| 155 | 10 | validity_bits | bitfield | 75 bits |

**Errors:** ERR_NOT_READY, ERR_INTERNAL

---

### `GET_FAULTS` — 0x0104

**Direction:** Host → Device  
**Purpose:** Full fault state.

**Request payload:** 0 bytes

**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 8 | active_faults | uint64_t LE |
| 8 | 8 | latched_faults | uint64_t LE |
| 16 | 4 | fault_event_count | Number of fault events since last clear |

**Errors:** ERR_INTERNAL

---

### `CLEAR_LATCHED_FAULTS` — 0x0105

**Direction:** Host → Device  
**Purpose:** Clear latched faults that are no longer active.

**Request payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 8 | fault_mask | Bitmask of faults to clear (uint64_t LE) |

**Response payload:** 8 bytes: `cleared_mask` — which faults were actually cleared  
**Errors:** ERR_WRONG_STATE (if underlying condition still active), ERR_ACCESS_DENIED

---

### `GET_DIAGNOSTICS_SUMMARY` — 0x0301

**Direction:** Host → Device  
**Purpose:** PEC error counters, open-wire results, ring buffer summary.

**Request payload:** 0 bytes

**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | cell_pec_error_count | Total PEC errors on CELL chain since reset |
| 2 | 2 | temp_pec_error_count | Total PEC errors on TEMP chain since reset |
| 4 | 2 | i2c_error_count | Total I2C errors |
| 6 | 1 | openwire_valid | 1 if last open-wire result is valid |
| 7 | 10 | openwire_bits | 75-bit mask; 1 = open-wire detected on that cell index |
| 17 | 2 | diag_event_count | Number of events in ring buffer |
| 19 | 1 | reset_cause | Last reset cause byte (from RCC flags) |

**Errors:** ERR_INTERNAL

---

### `GET_DIAGNOSTICS_LOG` — 0x0302

**Direction:** Host → Device  
**Purpose:** Stream diagnostic ring buffer.

**Request payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | offset | Entry offset from start of ring buffer |
| 2 | 1 | max_entries | Max entries to return (≤ 32) |

**Response payload:** Variable; array of `DiagEntry` (12 bytes each):

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | timestamp_ms | |
| 4 | 1 | event_type | |
| 5 | 1 | severity | |
| 6 | 4 | data | Event-specific |
| 10 | 2 | sequence | Global event sequence number |

---

### `RUN_OPENWIRE` — 0x0303

**Direction:** Host → Device  
**Purpose:** Trigger an open-wire diagnostic cycle on the CELL chain. Result available via GET_DIAGNOSTICS_SUMMARY.

**Request payload:** 0 bytes  
**Response payload:** 1 byte: `0x01` if started, `0x00` if busy  
**Errors:** ERR_WRONG_STATE (balancing must be disabled first), ERR_NOT_READY

---

### `GET_CONFIG` — 0x0201

**Direction:** Host → Device  
**Purpose:** Read current active config from device.

**Request payload:** 0 bytes  
**Response payload:** Full serialized `BmsConfig` struct (see `docs/05_config_schema.md`)  
**Length:** Fixed per schema version (see config schema for exact byte count)  
**Errors:** ERR_INTERNAL

---

### `VALIDATE_CONFIG` — 0x0202

**Direction:** Host → Device  
**Purpose:** Send a config blob for validation without storing it.

**Request payload:** Full serialized `BmsConfig` struct  
**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | result | 0x00 = valid, 0x01 = invalid |
| 1 | 2 | error_field_offset | Byte offset of first invalid field (0xFFFF if result valid) |
| 3 | 1 | error_code | Specific validation error code |
| 4 | 32 | error_message | Human-readable, null-terminated |

**Errors:** ERR_BAD_LENGTH, ERR_CRC_FAIL

---

### `SET_CONFIG_RAM` — 0x0203

**Direction:** Host → Device  
**Purpose:** Apply a config to RAM without persistent storage. Used for live testing. Reverts on reset.

**Request payload:** Full serialized `BmsConfig` struct  
**Response payload:** Same as `VALIDATE_CONFIG`  
**Errors:** ERR_CONFIG_INVALID, ERR_WRONG_STATE

---

### `STORE_CONFIG` — 0x0204

**Direction:** Host → Device  
**Purpose:** Validate + persistently write config to flash. Requires correct CRC in config struct.

**Request payload:** Full serialized `BmsConfig` struct  
**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | result | 0x00 = OK |
| 1 | 4 | stored_crc | CRC32 of written config (for host verification) |

**Errors:** ERR_CONFIG_INVALID, ERR_FLASH_FAIL, ERR_WRONG_STATE

---

### `GET_BOOT_INFO` — 0x0401

**Direction:** Host → Device  
**Purpose:** Get boot mode, bootloader version, available flash map.

**Request payload:** 0 bytes  
**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | boot_mode | 0x01 = application, 0x02 = bootloader |
| 1 | 3 | bootloader_version | [major, minor, patch] |
| 4 | 4 | app_start_addr | Provisional; see flash map |
| 8 | 4 | app_max_size | Max application image bytes |
| 12 | 4 | config_start_addr | |
| 16 | 4 | config_size | |
| 20 | 1 | update_in_progress | 1 if an incomplete update exists |

**Errors:** ERR_INTERNAL

---

### `ENTER_BOOTLOADER` — 0x0402

**Direction:** Host → Device (application mode only)  
**Purpose:** Request firmware to enter bootloader mode (sets boot flag, resets MCU).

**Request payload:** 4 bytes: `ENTER_BL_MAGIC = 0xB007B007`  
**Response payload:** 0 bytes (sent before reset; host waits for reconnect in bootloader mode)  
**Errors:** ERR_WRONG_STATE (if already in bootloader), ERR_NOT_BOOTLOADER (paradox — never sent)

---

### `BOOT_UPDATE_BEGIN` — 0x0403

**Direction:** Host → Bootloader (bootloader mode only)  
**Purpose:** Begin a firmware update session with package header validation.

**Request payload:** Firmware package header (see `docs/06_flash_and_bootloader.md` for exact fields; fixed 64 bytes)  
**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | result | 0x00 = accepted; 0x01 = rejected |
| 1 | 1 | reject_reason | ERR_* code if rejected |
| 2 | 4 | expected_chunk_size | Max chunk size bootloader will accept |
| 6 | 4 | total_chunks | ceil(app_size / chunk_size) |

**Errors:** ERR_WRONG_TARGET, ERR_PACKAGE_INVALID, ERR_NOT_BOOTLOADER

---

### `BOOT_UPDATE_CHUNK` — 0x0404

**Direction:** Host → Bootloader  
**Purpose:** Send one chunk of firmware payload.

**Request payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | chunk_index | 0-based |
| 4 | 4 | chunk_len | Actual bytes in this chunk |
| 8 | N | data | Chunk bytes |

**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | result | 0x00 = OK, 0x01 = retry, 0x02 = abort |
| 1 | 4 | next_expected_chunk | |

**Errors:** ERR_SEQUENCE, ERR_FLASH_FAIL, ERR_NOT_BOOTLOADER

---

### `BOOT_UPDATE_FINALIZE` — 0x0405

**Direction:** Host → Bootloader  
**Purpose:** Signal end of transfer; bootloader verifies CRC32 of full written image.

**Request payload:** 0 bytes  
**Response payload:**

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | result | 0x00 = verified and will boot new image; 0x01 = CRC mismatch |
| 1 | 4 | computed_crc | CRC32 of written image |

On success, bootloader boots the new application on next reset or immediately.  
**Errors:** ERR_NOT_BOOTLOADER, ERR_PACKAGE_INVALID

---

### `BOOT_UPDATE_ABORT` — 0x0406

**Direction:** Host → Bootloader  
**Purpose:** Abort in-progress update; discard partial image.

**Request payload:** 0 bytes  
**Response payload:** 1 byte: `0x00` = aborted  
**Errors:** ERR_NOT_BOOTLOADER

---

## 7. Capabilities-First Rules

The tool must:
1. Send `GET_CAPABILITIES` first
2. Match `hw_profile_id` against known supported profiles
3. Match `protocol_version` against tool's minimum supported version
4. If `firmware_type == 0x0002` (bootloader): only `GET_BOOT_INFO`, `BOOT_UPDATE_*`, `BOOT_UPDATE_ABORT` are valid
5. Never send `STORE_CONFIG` or `BOOT_UPDATE_*` before receiving a valid capabilities response
6. Never send config for a schema version the device does not support

---

## 8. Compatibility / Version Rules

| Condition | Behaviour |
|---|---|
| `protocol_version` in capabilities < tool minimum | Tool refuses all operations; displays incompatibility error |
| Tool protocol version < device minimum | Device returns ERR_WRONG_STATE on first non-capabilities packet |
| Config schema version mismatch | VALIDATE_CONFIG returns ERR_CONFIG_INVALID with detail |
| hw_profile_id mismatch in firmware package | Bootloader returns ERR_WRONG_TARGET |

Protocol version is incremented on any breaking change to packet format or semantics. The `minimum_protocol_version` in each packet definition marks the version that introduced it.
