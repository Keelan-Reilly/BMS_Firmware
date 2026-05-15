# 08 — Validation Plan

## 1. Test Folder Structure

```
tests/
  unit/
    test_pec15.c              # LTC6812 PEC calculation
    test_protocol_crc.c       # Protocol FRAME_CRC (CRC-16/CCITT)
    test_config_crc.c         # Config CRC32
    test_config_validate.c    # Config schema validation logic
    test_config_masks.c       # Mask high-bit validation
    test_config_thresholds.c  # Threshold ordering validation
    test_bms_faults.c         # Fault evaluation logic
    test_bms_balance.c        # Balance gating rules
    test_bms_state.c          # State machine transitions
    test_bms_measurements.c   # Measurement validity / stale logic
    test_bms_outputs.c        # Permission gating matrix
    test_isospi_framing.c     # isoSPI wake/read/write frame construction
    test_ltc6812_commands.c   # LTC6812 command opcode encoding
    test_vpack_cal.c          # Vpack calibration math
    test_isl28022_parse.c     # ISL28022 register parsing
  golden/
    packets/
      get_capabilities_req.bin / get_capabilities_resp.bin
      get_cells_req.bin / get_cells_resp.bin
      get_temps_req.bin / get_temps_resp.bin
      get_faults_req.bin / get_faults_resp.bin
      store_config_valid.bin
      store_config_invalid_crc.bin
      boot_update_begin_valid.bin
      boot_update_begin_wrong_hw.bin
    configs/
      valid_config_v1.bin
      invalid_magic.bin
      invalid_crc.bin
      invalid_schema_version.bin
      invalid_mask_oob.bin
      invalid_threshold_order.bin
      invalid_hw_profile.bin
    packages/
      valid_package.pkg
      wrong_hw_profile.pkg
      bad_header_crc.pkg
      bad_payload_crc.pkg
      bad_sp_value.pkg
      bad_reset_vector.pkg
  fuzz/
    fuzz_protocol_rx.c        # Feed random bytes to protocol parser
    fuzz_config_validate.c    # Feed random blobs to config validator
  integration/
    test_fake_target.py       # Tool ↔ fake_target protocol tests
    test_config_roundtrip.py  # Write config → read back → compare
    test_update_flow.py       # Full bootloader update flow with fake_target
  hardware/
    test_bring_up_sequence.md   # Manual test script for hardware bring-up
    test_hardware_acceptance.md # Acceptance checklist
  CMakeLists.txt
```

---

## 2. Unit Tests

All unit tests compile and run on the host (no embedded target required). Mock BSP replaces hardware drivers.

### 2.1 LTC6812 PEC Tests

| Test | Description |
|---|---|
| `pec_known_vector_1` | Compute PEC15 for datasheet example bytes; verify matches datasheet |
| `pec_known_vector_2` | Second datasheet example |
| `pec_single_byte` | PEC of single byte |
| `pec_empty` | PEC of zero bytes → known result |
| `pec_all_zeros` | PEC of 6 zero bytes → known result |
| `pec_bit_flip` | Change one bit in data; verify PEC changes |

Reference vectors from LTC6812 datasheet §8 / Linduino `pec15_calc` source.

### 2.2 Protocol CRC Tests

| Test | Description |
|---|---|
| `frame_crc_known_vector` | Known byte sequence → known CRC-16/CCITT result |
| `frame_crc_all_zeros` | |
| `frame_crc_sof_only` | SOF + empty payload |
| `frame_crc_detect_byte_flip` | Flip any byte; verify CRC fails |
| `frame_crc_detect_byte_drop` | Drop any byte; verify CRC fails |
| `frame_crc_max_payload` | 1024-byte payload; CRC computed correctly |

### 2.3 Config Validation Tests

| Test | Input | Expected Result |
|---|---|---|
| `valid_config_passes` | Well-formed config v1 | CFG_LOAD_OK |
| `wrong_magic` | magic != 0xBBCC0001 | ERR_MAGIC |
| `wrong_length` | total_length off by 1 | ERR_LENGTH |
| `wrong_schema_version` | schema_version = 2 | ERR_SCHEMA_VERSION |
| `wrong_hw_profile` | hw_profile_id = 0xFFFF | ERR_HW_PROFILE |
| `bad_crc` | Flip a byte in payload | ERR_CRC |
| `mask_oob_cell` | required_cell_mask bit 75 set | ERR_MASK_OOB |
| `mask_oob_bit79` | required_cell_mask bit 79 set | ERR_MASK_OOB |
| `threshold_ov_lt_uv` | cell_ov_hard_mv < cell_uv_hard_mv | ERR_THRESHOLD_ORDER |
| `threshold_soft_ov_lt_hard_uv` | cell_ov_soft_mv < cell_uv_hard_mv | ERR_THRESHOLD_ORDER |
| `temp_charge_hard_lt_warn` | temp_charge_hard_cx10 < temp_charge_warn_cx10 | ERR_THRESHOLD_ORDER |
| `reserved_nonzero` | Any reserved byte != 0 | ERR_RESERVED |
| `precharge_pct_0` | precharge_pct = 0 | ERR_RANGE |
| `precharge_pct_100` | precharge_pct = 100 | ERR_RANGE |
| `stale_timeout_zero` | stale_data_timeout_ms = 0 | ERR_RANGE |
| `generation_overflow` | config_generation = 0xFFFFFFFF | ERR_RANGE |

### 2.4 Fault Evaluation Tests

One test per fault bit: inject exact condition → verify bit set. Also:
- Condition resolves: active fault clears; latched fault persists
- Explicit clear of latched: clears only if active condition false
- Multiple simultaneous faults: all bits correctly set
- `FAULT_CONFIG_INVALID`: set on bad config load; clears when valid config stored

### 2.5 Permission Gating Tests

For each row in the permission gating matrix (`docs/02_safety_model.md §7`):
- Set the fault/condition → verify each blocked permission is deasserted
- Clear the fault/condition → verify permission can be asserted (if state allows)

Specifically: `FAULT_TEMP_CHAIN_BALANCE_ATTEMPT` → verify FATAL, all permissions deasserted.

### 2.6 Balance Gating Tests

| Test | Description |
|---|---|
| `balance_blocked_invalid_cell_read` | `FAULT_CELL_READ_INVALID` → balance mask = all zero |
| `balance_blocked_openwire` | `FAULT_CELL_OPENWIRE` → balance disabled |
| `balance_blocked_wrong_state` | State != BALANCING → balance disabled |
| `balance_allowed_mask_enforced` | Cell not in allowed mask → DCC bit 0 even if voltage high |
| `balance_no_temp_chain_write` | Mock ltc6812: verify DCC write never called on CHAIN_TEMP |
| `balance_correct_threshold` | Cell above target+hysteresis → DCC set; below → cleared |

---

## 3. Protocol Golden Tests

Binary golden files in `tests/golden/packets/`. Test process:
1. Load request `.bin` → feed to protocol parser → verify dispatch
2. Generate response → serialize → compare against response `.bin` byte-for-byte

| Golden File | Description |
|---|---|
| `get_capabilities_*` | Request + expected response for known firmware state |
| `get_cells_*` | All 75 cells at known values; exact byte-level comparison |
| `store_config_valid_*` | Full valid config roundtrip |
| `store_config_invalid_crc_*` | ERR_CONFIG_INVALID response |

CRC values in golden files are pre-computed and fixed. Regenerate with `scripts/gen_golden.py`.

---

## 4. Firmware Package Tests

| Test | Input | Expected |
|---|---|---|
| `valid_package` | Correct header + payload | Accepted |
| `wrong_magic` | pkg_magic = 0x00000000 | ERR_PACKAGE_INVALID |
| `wrong_hw_profile` | hw_profile_id = 0xFFFF | ERR_WRONG_TARGET |
| `bad_header_crc` | Flip a byte in header | ERR_PACKAGE_INVALID |
| `bad_payload_crc` | Flip a byte in payload | ERR_PACKAGE_INVALID (after transfer) |
| `bad_sp_in_image` | image[0] = 0x00000000 | ERR_PACKAGE_INVALID (vector check) |
| `bad_reset_vector` | image[1] = 0x00000000 | ERR_PACKAGE_INVALID |
| `oversized_image` | app_size > APP_REGION_SIZE | ERR_PACKAGE_INVALID |
| `zero_size_image` | app_size = 0 | ERR_PACKAGE_INVALID |
| `wrong_pkg_version` | pkg_version = 255 | ERR_PACKAGE_INVALID |

---

## 5. Flash Layout Tests (Host-Compiled)

| Test | Description |
|---|---|
| `no_region_overlap` | Assert CONFIG_A, CONFIG_B, APP, BL, STAGING regions do not overlap |
| `app_region_page_aligned` | APP_START_ADDR % 2048 == 0 |
| `config_region_page_aligned` | CONFIG_A_ADDR % 2048 == 0 |
| `bootloader_fits_in_region` | sizeof(bootloader_image) < BL_REGION_SIZE |
| `config_fits_in_slot` | sizeof(BmsConfig) ≤ CONFIG_SLOT_SIZE |
| `total_flash_not_exceeded` | Sum of all regions ≤ FLASH_SIZE |

---

## 6. Bootloader Validation Tests

| Test | Description |
|---|---|
| `valid_image_jump` | Mock flash with valid image; verify jump attempted |
| `invalid_sp_no_jump` | SP out of range; verify stays in bootloader |
| `invalid_rv_no_jump` | Reset vector out of range; verify stays in bootloader |
| `crc_fail_no_jump` | Image CRC mismatch; verify stays in bootloader |
| `bl_entry_flag_set` | Boot flag set in retained SRAM → stays in bootloader; clears flag |
| `config_preserved` | Write firmware; verify config region unchanged |
| `bad_chunk_sequence` | Send chunk_index = 5 before 0–4 → ERR_SEQUENCE |
| `power_loss_recovery` | Simulate partial flash write; verify bootloader enters recovery |

---

## 7. Desktop Fake Target Tests (`tests/integration/`)

All run against `fake_target.py`:

| Test | Description |
|---|---|
| `capabilities_handshake` | Tool connects; receives valid capabilities |
| `unknown_target` | fake_target does not respond; tool enters UNKNOWN_TARGET mode |
| `wrong_protocol_version` | capabilities has old version; tool shows UNSUPPORTED_TARGET |
| `config_read_roundtrip` | GET_CONFIG → modify field → VALIDATE_CONFIG → STORE_CONFIG → GET_CONFIG → verify |
| `config_invalid_rejected` | Send config with bad CRC → ERR_CONFIG_INVALID |
| `config_wrong_hw_profile` | Send config with wrong hw_profile_id → ERR_WRONG_TARGET |
| `bootloader_entry_flow` | ENTER_BOOTLOADER → reconnect → GET_CAPABILITIES in BL mode → UPDATE_BEGIN |
| `update_full_flow` | Complete update cycle with fake firmware package |
| `update_wrong_hw_abort` | Package with wrong hw_profile_id → ERR_WRONG_TARGET |
| `update_abort_mid_transfer` | Abort after 3 chunks → verify clean state |
| `fault_clear_flow` | Inject fault in fake_target → GET_FAULTS → CLEAR_LATCHED_FAULTS → verify |

---

## 8. Hardware-in-Loop Tests

Requires: STM32F303 board + LTC6812 evaluation or production hardware + ST-Link.

| Test | Description | Hardware Required |
|---|---|---|
| HIL-01 | isoSPI CELL chain wake + RDCFG round-trip | Board + LTC6812 chain |
| HIL-02 | ADCV + RDCV all 75 cells | Board + cell simulator or real cells |
| HIL-03 | PEC error injection (corrupt MOSI bit) | Logic analyser / signal manipulation |
| HIL-04 | TEMP chain ADAX + RDAUX all 75 sensors | Board + temperature sensors |
| HIL-05 | TEMP S outputs cycle: assert → settle → read → deassert | Oscilloscope on S-output pins |
| HIL-06 | Balance DCC: enable → verify discharge via scope/load | Board + discharge resistors |
| HIL-07 | ISL28022 I2C: Vbat read within 2% of DMM reference | Board + reference PSU |
| HIL-08 | PA1 Vpack ADC calibration | Board + calibrated voltage source |
| HIL-09 | Permission outputs: assert each, measure GPIO level | Logic probe on PB11/PB10/PB0/PB2 |
| HIL-10 | Power latch: release POWER_ENABLE; verify board powers off | Real board |
| HIL-11 | Watchdog reset: stall main loop → verify reset occurs within timeout | Debug probe |
| HIL-12 | Config write/read persistence across power cycle | Real board |
| HIL-13 | Bootloader entry via protocol → package update → verify new fw runs | Real board + ST-Link |
| HIL-14 | Open-wire diagnostic: disconnect a cell tap → verify fault | Real board |
| HIL-15 | Simultaneous CS_CELL/CS_TEMP guard: verify only one CS active at a time | Logic analyser on PA4/PB12 |

---

## 9. Bench Bring-Up Test Sequence

Perform these in order on first hardware bring-up:

1. **Power:** Apply 3.3V; measure supply; confirm LM5165 output stable.
2. **MCU alive:** ST-Link connect; confirm `DBGMCU_IDCODE` readable; flash LED blink firmware.
3. **Clock:** Verify sysclk via SysTick toggle on a spare GPIO.
4. **Permission outputs safe:** Confirm PB11/PB10/PB0/PB2 all at safe (de-asserted) voltage after reset.
5. **UART:** Flash BMS firmware; confirm "BMS BOOT" log appears on USB/UART at 115200.
6. **isoSPI CELL:** `GET_DIAGNOSTICS_SUMMARY` via tool; verify zero PEC errors after RDCFG round-trip.
7. **Cell voltages:** Apply known voltage to one cell tap; verify correct reading in tool.
8. **isoSPI TEMP:** Verify ADAX conversion completes; verify RDAUX data non-zero.
9. **Temperature conversion:** Verify temperature reading within ±2°C of reference thermometer.
10. **ISL28022:** Verify Vbat reading matches bench PSU voltage within 1%.
11. **PA1 ADC:** Verify Vpack reading after calibration.
12. **Balancing:** Enable one cell; scope across DCC discharge path; confirm current flow.
13. **Open-wire:** Disconnect cell tap; confirm `FAULT_CELL_OPENWIRE` in tool.
14. **CAN:** Verify CAN frames received by CANalyzer/oscilloscope.
15. **Bootloader update:** Flash known firmware package via tool update flow; confirm new version in capabilities.

---

## 10. Acceptance Checklist

- [ ] All unit tests pass on host
- [ ] All golden tests match byte-for-byte
- [ ] All flash layout tests pass
- [ ] All firmware package validation tests pass
- [ ] Fake target integration tests all pass
- [ ] No compiler warnings with `-Wall -Wextra -Werror`
- [ ] HIL-01 through HIL-15 all pass on hardware
- [ ] Config write/read roundtrip verified on hardware
- [ ] Bootloader update tested end-to-end on hardware
- [ ] TEMP chain DCC write attempted → FAULT_TEMP_CHAIN_BALANCE_ATTEMPT triggered (inject via test hook)
- [ ] Power cycle: all permissions deasserted immediately after reset (scope probe)
- [ ] Watchdog timeout demonstrated (stall test firmware branch)
- [ ] Invalid config at boot → FAULT_CONFIG_INVALID set; monitoring still works

---

## 11. What Can Be Tested Without Hardware

Everything in §2–§6: unit tests, golden tests, flash layout tests, package validation tests, fake target integration tests. These require only a host C compiler and Python.

---

## 12. What Requires Hardware

All HIL tests (§8) and the bench bring-up sequence (§9). Additionally:
- Real isoSPI timing validation (correct wake/sleep/CS timing)
- Real PEC error behaviour under real SPI noise
- Real temperature sensor V-T curve verification against Enepaq datasheet
- Permission output polarity confirmation
- POWER_ENABLE / power-latch verification
- CAN bus electrical validation
