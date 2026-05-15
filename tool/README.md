# Desktop Tool

BMS monitoring, configuration, and firmware update tool for the BMS system.

## Architecture

**Stack:** Python 3.11+ / PyQt6 / pyserial  
**Pattern:** Single-window multi-page application with a central connection state machine

```
tool/
  src/
    main.py                  Entry point; creates app + main window
    connection/
      connection_manager.py  Serial port open/close; GET_CAPABILITIES; mode tracking
      device_state.py        CapabilitiesState, DeviceMode enum, current device info
    protocol/
      framing.py             Frame encode/decode: SOF, PKT_ID, FLAGS, SEQ, LEN, CRC-16
      crc.py                 CRC-16/CCITT-FALSE implementation
      client.py              Request/response send with timeout and retry
      packet_defs.py         Packet ID constants (generated from protocol/packet_ids.yaml)
    pages/
      connection_page.py
      dashboard_page.py
      cells_page.py
      temperatures_page.py
      faults_page.py
      config_page.py
      diagnostics_page.py
      firmware_flash_page.py
      logs_page.py
    config/
      schema.py              Config struct serialize/deserialize (generated from config_schema.yaml)
      validator.py           Client-side config validation (mirrors firmware validation logic)
      editor_widget.py       Auto-generated config editor UI from schema
    update/
      package_parser.py      Firmware package header parse and validate
      stlink_flasher.py      Subprocess wrapper for STM32_Programmer_CLI
      bootloader_updater.py  BOOT_UPDATE_BEGIN → chunk loop → FINALIZE flow
    fake_target/
      fake_target.py         Simulated BMS device; implements full request/response protocol
      scenarios/             YAML scenario files for fake_target state injection
    utils/
      log_manager.py         Persistent log with rotation and export
      bit_grid_widget.py     75-bit visual grid widget (reused for masks, open-wire, cell/temp grid)
  scripts/
    gen_protocol_consts.py   Generate packet_defs.py from protocol/packet_ids.yaml
    gen_config_schema.py     Generate schema.py from protocol/config_schema.yaml
  tests/
    test_framing.py          CRC, encode/decode, error injection
    test_config_validator.py Client-side validation mirrors firmware validation
    test_fake_target.py      Full integration flow against fake_target
    test_package_parser.py   Package header parse and validation
  requirements.txt
  setup.py / pyproject.toml
  .github/workflows/ci.yml
```

---

## UI Architecture

- **Single QMainWindow** with a `QStackedWidget` for pages
- **Left sidebar navigation** (icons + labels) — disabled pages grayed out based on `DeviceMode`
- **Connection status bar** at top: port name, mode badge, firmware version
- Each page is a `QWidget` subclass; communicates with the backend via Qt signals/slots
- **Data refresh:** background `QThread` polls `GET_VALUES` and `GET_CELLS`/`GET_TEMPS` at configured rate; emits signals to update UI
- **Non-blocking serial I/O:** all serial operations run in a worker thread; UI updates via `pyqtSignal`

---

## Protocol Client Structure

```python
class BmsProtocolClient:
    def __init__(self, port: serial.Serial)
    def send_request(self, pkt_id: int, payload: bytes) -> bytes  # blocking, with timeout/retry
    def get_capabilities(self) -> CapabilitiesResponse
    def get_values(self) -> ValuesResponse
    def get_cells(self, include_validity: bool) -> GetCellsResponse
    def get_temps(self) -> GetTempsResponse
    def get_faults(self) -> GetFaultsResponse
    def clear_latched_faults(self, mask: int) -> int
    def get_config(self) -> BmsConfig
    def validate_config(self, cfg: BmsConfig) -> ConfigValidationResponse
    def set_config_ram(self, cfg: BmsConfig) -> ConfigValidationResponse
    def store_config(self, cfg: BmsConfig) -> StoreConfigResponse
    def enter_bootloader(self) -> None
    def get_boot_info(self) -> BootInfoResponse
    def boot_update_begin(self, header: FirmwarePackageHeader) -> BootUpdateBeginResponse
    def boot_update_chunk(self, index: int, data: bytes) -> BootUpdateChunkResponse
    def boot_update_finalize(self) -> BootUpdateFinalizeResponse
    def boot_update_abort(self) -> None
```

All methods raise `ProtocolError` subclasses on timeout, bad CRC, or error response.

---

## Fake Target Strategy

`fake_target.py` runs as either:
1. A subprocess listening on a virtual serial port pair (for tool integration tests)
2. An in-process mock (for unit tests of protocol client)

Scenario files (`scenarios/*.yaml`) define:
- Cell voltages (array of 75 values or a pattern)
- Temperature values (array of 75 values)
- Active faults bitmask
- Latched faults bitmask
- State
- Whether to inject PEC errors

The fake target is the primary CI test vehicle. All tool flows are tested against it in GitHub Actions before any hardware is available.

---

## Config Editor Strategy

`config/editor_widget.py` is **generated** from `protocol/config_schema.yaml` by `scripts/gen_config_schema.py`. The generator produces:
- A `QFormLayout`-based editor with one row per config field
- Type-appropriate input widgets (QSpinBox, QDoubleSpinBox, QLineEdit for masks)
- Range hints shown as placeholder/tooltip text
- Red highlight on out-of-range values
- Threshold ordering violations shown inline

To add a config field: edit `protocol/config_schema.yaml` → run `gen_config_schema.py` → rebuild tool.

---

## Firmware Flashing Strategy

**ST-Link path (development):**
```python
class StLinkFlasher:
    def detect(self) -> list[StLinkInfo]  # calls STM32_Programmer_CLI --list
    def flash(self, path: str, address: int, on_progress: Callable) -> FlashResult
    # Runs: STM32_Programmer_CLI -c port=SWD -d <path> <address> -v -rst
```

**Bootloader path (production/service):**
```python
class BootloaderUpdater:
    def __init__(self, client: BmsProtocolClient)
    def update(self, pkg_path: str, on_progress: Callable[[int, int], None]) -> UpdateResult
    # Validates package, enters bootloader, chunks, finalizes, reconnects
```

Both operate on real `BmsProtocolClient` or `StLinkFlasher` — the UI just calls these and displays progress.

---

## Packaging

PyInstaller one-folder bundle:
```bash
pyinstaller bms_tool.spec
```

Output: `dist/BmsTool/` (folder) or `dist/BmsTool` (single exe with `--onefile`).

CI builds produce signed packages for macOS and unsigned installer for Windows. Linux users can run from source.

Version baked in from `git describe --tags`.

---

## Test Approach

1. `pytest tests/test_framing.py` — unit test packet encode/decode, CRC, error injection
2. `pytest tests/test_config_validator.py` — client-side validation matches firmware validation
3. `pytest tests/test_fake_target.py` — full protocol flows (requires `fake_target.py` subprocess)
4. `pytest tests/test_package_parser.py` — package header parse, bad-header detection

All tests run in CI without hardware. Hardware-in-loop tests are manual (see `docs/08_validation_plan.md`).
