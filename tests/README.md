# Tests

Test infrastructure for the BMS system. Tests are organized by layer and can be run at different levels of hardware dependency.

---

## Folder Structure

```
tests/
  unit/                     Host-compiled unit tests (no hardware required)
    test_pec15.c
    test_protocol_crc.c
    test_config_crc.c
    test_config_validate.c
    test_config_masks.c
    test_config_thresholds.c
    test_bms_faults.c
    test_bms_balance.c
    test_bms_state.c
    test_bms_measurements.c
    test_bms_outputs.c
    test_isospi_framing.c
    test_ltc6812_commands.c
    test_vpack_cal.c
    test_isl28022_parse.c
    test_bootloader_validate.c
    test_bootloader_boot_decision.c
    test_flash_layout.c
  golden/                   Binary reference files for packet/config/package tests
    packets/
      *.req.bin / *.resp.bin
    configs/
      valid_config_v1.bin
      invalid_*.bin
    packages/
      valid_package.pkg
      bad_*.pkg
  fuzz/                     Fuzz targets (libFuzzer or AFL compatible)
    fuzz_protocol_rx.c
    fuzz_config_validate.c
    fuzz_package_validate.c
  integration/              Python integration tests (host, uses fake_target)
    test_fake_target.py
    test_config_roundtrip.py
    test_update_flow.py
    test_fault_clear_flow.py
    test_capabilities_modes.py
  hardware/                 Hardware-in-loop test scripts and documentation
    test_bring_up_sequence.md
    test_hardware_acceptance.md
    hil_cell_voltages.py      HIL automation script (if logic analyser/instrument supported)
  mock_bsp/                 Mock board support layer for host-compiled tests
    mock_board_spi.c/h
    mock_board_i2c.c/h
    mock_board_uart.c/h
    mock_board_outputs.c/h
    mock_board_adc.c/h
    mock_board_clock.c/h
    mock_board_flash.c/h
  vendor/
    unity/                  Unity test framework (single-file C)
  CMakeLists.txt
  pytest.ini
```

---

## Protocol / Config Golden Tests

Golden tests compare serialized binary blobs byte-for-byte against known-good reference files.

**Generating golden files:**
```bash
python scripts/gen_golden.py
```
This script uses the Python config serializer and packet encoder to produce reference binaries for all golden test cases. Golden files are committed to the repo.

**Running golden tests:**
```bash
ctest --test-dir build_test -L golden
```

Golden tests catch:
- Field offset regressions (if a field shifts, the golden binary will differ)
- CRC computation regressions
- Endian regressions

**Do not hand-edit golden binary files.** Regenerate with `gen_golden.py` and commit the diff when the schema intentionally changes.

---

## Fake Target Tests

`tests/integration/test_fake_target.py` exercises the full tool ↔ fake_target protocol flow.

```bash
# Start fake_target in background, run tests
pytest tests/integration/ -v
```

The fake target runs as a subprocess on a virtual serial port pair (`socat` or `com0com` on Windows).

### Key scenarios tested:

| Test File | Scenarios |
|---|---|
| `test_capabilities_modes.py` | BMS mode, bootloader mode, unknown target, unsupported protocol version |
| `test_config_roundtrip.py` | Write valid config → read back → compare; write invalid config → ERR_CONFIG_INVALID |
| `test_update_flow.py` | Full update (BEGIN → chunks → FINALIZE); wrong hw_profile → reject; abort mid-transfer |
| `test_fault_clear_flow.py` | Inject fault → GET_FAULTS → CLEAR_LATCHED (active) → verify not cleared; resolve → clear → verify cleared |
| `test_fake_target.py` | GET_VALUES, GET_CELLS, GET_TEMPS all return expected fake data |

---

## Firmware Package Tests

```bash
ctest --test-dir build_test -L package
```

Tests in `unit/test_bootloader_validate.c` use the `bl_validate` module with a mock flash array:
- Each test case has a pre-built package header (in `golden/packages/`)
- Test injects the package into mock flash, calls `bl_validate_package_header()`
- Verifies return code

---

## Flash Layout Tests

`unit/test_flash_layout.c` verifies at compile time (static asserts) and at run time (test assertions):
- No region overlap
- All regions page-aligned
- BMS config struct fits in config slot
- Bootloader binary fits in bootloader region (requires a build step to get bootloader size)
- Total flash does not exceed `FLASH_SIZE_BYTES`

This test file includes `bl_config.h` and `bms_constants.h` and asserts all the invariants. It must be updated whenever the flash map changes.

---

## CI Plan

**GitHub Actions workflow:** `.github/workflows/ci.yml`

```yaml
jobs:
  build_firmware:
    - arm-none-eabi-gcc toolchain
    - cmake --build
    - Run static analysis (cppcheck)

  unit_tests:
    - cmake -DBMS_BUILD_TESTS=ON
    - ctest --output-on-failure
    - Coverage report (gcov/lcov)

  python_tests:
    - pytest tests/integration/ tests/unit/ (Python tests)
    - socat required for virtual serial pair

  package_build:
    - Build firmware .bin
    - Run gen_package.py
    - Validate generated .pkg with package parser
    - Compare against golden

  golden_check:
    - Run gen_golden.py
    - git diff --exit-code golden/  (fail if any golden file changed without being committed)
```

PRs must pass all jobs. Flash to hardware is a separate manual step (not in CI).

---

## Hardware-in-Loop Plan

HIL tests require:
- STM32F303 board with LTC6812 hardware
- ST-Link for flashing
- Lab power supply (for controlled cell voltages / Vbat)
- Oscilloscope (for timing verification)
- Thermometer (for temperature calibration)

HIL tests are run manually by following `hardware/test_bring_up_sequence.md` and `hardware/test_hardware_acceptance.md`.

Automation potential (future):
- Python control of a Keysight/Rigol PSU for voltage injection
- Python readback from oscilloscope for timing verification
- These would be wired into a dedicated HIL machine, not CI

---

## What Can Be Tested Without Hardware

| Category | Tests |
|---|---|
| PEC15 calculation | ✓ Unit test |
| Protocol CRC | ✓ Unit test |
| Config CRC | ✓ Unit test |
| Config validation (all rules) | ✓ Unit test |
| Fault evaluation logic | ✓ Unit test |
| Permission gating matrix | ✓ Unit test |
| Balance gating logic | ✓ Unit test |
| State machine transitions | ✓ Unit test |
| Measurement stale logic | ✓ Unit test |
| isoSPI frame construction | ✓ Unit test |
| LTC6812 command encoding | ✓ Unit test |
| ISL28022 register parsing | ✓ Unit test |
| Flash layout overlap | ✓ Unit test |
| Bootloader validation logic | ✓ Unit test |
| Protocol golden tests | ✓ Golden test |
| Full tool ↔ fake_target flows | ✓ Integration test |
| Config roundtrip | ✓ Integration test |
| Update flow | ✓ Integration test |

---

## What Requires Hardware

- isoSPI timing verification (CS-to-SCK, SCK-to-CS hold times)
- Real PEC error behaviour under SPI noise
- Temperature sensor V-T calibration
- Permission output polarity confirmation
- Vpack ADC calibration
- Current measurement calibration
- Power-latch sequencing
- CAN bus electrical validation
- IWDG timeout demonstration
- End-to-end bootloader update
- Open-wire detection with real disconnected cell
