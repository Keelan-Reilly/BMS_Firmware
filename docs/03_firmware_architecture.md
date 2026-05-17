# 03 — Firmware Architecture

## 1. Folder Structure

```
firmware/
  src/
    bsp/                    # Board support: pin, clock, peripheral init
      board_pins.c/h
      board_clock.c/h
      board_flash.c/h
      board_outputs.c/h
      board_adc.c/h
      board_i2c.c/h
      board_spi.c/h
      board_uart.c/h
      board_can.c/h
    drivers/                # Device drivers (no BMS logic)
      ltc6812.c/h
      isospi.c/h            # isoSPI transport layer
      isl28022.c/h
      vpack_adc.c/h
    bms/                    # BMS application logic
      bms_measurements.c/h
      bms_faults.c/h
      bms_outputs.c/h
      bms_state.c/h
      bms_balance.c/h
      bms_config.c/h
      bms_protocol.c/h
      bms_diagnostics.c/h
      bms_can.c/h
      bms_update_interface.c/h
    main.c
    bms_main_loop.c/h
  include/
    bms_types.h             # Shared types: enums, result codes, measurement structs
    bms_constants.h         # Cross-layer constants (protocol version, cell count, etc.)
    bms_error.h
  tests/                    # Unit tests (host-compiled)
    ...
  CMakeLists.txt
  linker/
    stm32f303_bms.ld
```

---

## 2. Single Source of Truth

All cross-layer constants are defined in **one** location:

| Constant | Defined In | Used By |
|---|---|---|
| `CELL_COUNT` = 75 | `bms_constants.h` | firmware, protocol generation, config schema |
| `TEMP_COUNT` = 75 | `bms_constants.h` | same |
| `CELL_IC_COUNT` = 5 | `bms_constants.h` | ltc6812 driver |
| `TEMP_IC_COUNT` = 5 | `bms_constants.h` | ltc6812 driver |
| `CELLS_PER_IC` = 15 | `bms_constants.h` | ltc6812 driver |
| `PROTOCOL_VERSION` = 1 | `bms_constants.h` | bms_protocol, capabilities |
| `CONFIG_SCHEMA_VERSION` = 1 | `bms_constants.h` | bms_config |
| `HW_PROFILE_ID` = TBD | `bms_constants.h` | bms_config, bootloader |

`bms_constants.h` is also the source for generated YAML/config schema — a build tool or Python script generates `protocol/config_schema.yaml` header sections from it.

---

## 3. Module Specifications

### 3.1 `board_pins`

| Item | Detail |
|---|---|
| Responsibility | Configure all STM32 GPIO directions, alternate functions, pull-up/down, speed |
| Owned state | None |
| Public API | `board_pins_init(void)` |
| Must not do | Any logic; no safety decisions |
| Notes | All permission pins initialized as outputs in safe state BEFORE this function returns |

```c
void board_pins_init(void);
```

---

### 3.2 `board_clock`

| Item | Detail |
|---|---|
| Responsibility | Configure HSE/PLL, SYSCLK, AHB/APB dividers, SysTick, enable peripheral clocks |
| Owned state | System tick counter (millisecond resolution) |
| Public API | `board_clock_init(void)`, `board_millis(void) → uint32_t`, `board_delay_ms(uint32_t)` |
| Must not do | BMS logic; long blocking delays |
| Notes | IWDG must be configured here or in `main.c` before any sensor initialization |

```c
void board_clock_init(void);
uint32_t board_millis(void);
void board_delay_ms(uint32_t ms);
void board_watchdog_kick(void);
```

---

### 3.3 `board_flash`

| Item | Detail |
|---|---|
| Responsibility | Erase/write/read STM32 internal flash pages; CRC verification of written data |
| Owned state | None (operates on passed addresses) |
| Public API | Read, write page, erase page; verify CRC |
| Must not do | BMS logic; wear levelling (v1 uses simple dual-slot config) |
| Notes | All writes require unlock sequence; re-lock after each write |

```c
FlashResult board_flash_read(uint32_t addr, void *buf, uint32_t len);
FlashResult board_flash_write_page(uint32_t addr, const void *buf, uint32_t len);
FlashResult board_flash_erase_page(uint32_t addr);
uint32_t board_flash_crc32(uint32_t addr, uint32_t len);
```

---

### 3.4 `board_outputs`

| Item | Detail |
|---|---|
| Responsibility | Abstract permission GPIO writes; owns active polarity constants |
| Owned state | Current asserted state of each output (for readback/verification) |
| Public API | Set/clear/query each named output; bulk safe-deassert |
| Must not do | Apply any logic about when to assert; that belongs in `bms_outputs` |
| Notes | Active polarity (`HIGH=asserted` or `LOW=asserted`) defined here as compile-time constants after schematic confirmation |

```c
typedef enum {
    OUTPUT_MASTER_OK,
    OUTPUT_DISCHARGE_PERM,
    OUTPUT_CHARGE_PERM,
    OUTPUT_CHARGER_SAFETY,
    OUTPUT_POWER_ENABLE,
    OUTPUT_COUNT
} OutputId;

typedef enum { OUTPUT_DEASSERTED = 0, OUTPUT_ASSERTED = 1 } OutputState;

void board_outputs_init(void);              // All outputs → DEASSERTED
void board_output_set(OutputId id, OutputState state);
OutputState board_output_get(OutputId id);
void board_outputs_deassert_all(void);      // Emergency safe-state
```

---

### 3.5 `board_adc`

| Item | Detail |
|---|---|
| Responsibility | Configure and read STM32 ADC1 channel for Vpack (PA1/IN2) |
| Owned state | None |
| Public API | `board_adc_init(void)`, `board_adc_read_vpack_raw(uint16_t *raw)` |
| Must not do | Voltage scaling; that belongs in `vpack_adc` |
| Notes | Calibration run at init; 12-bit result |

```c
void board_adc_init(void);
AdcResult board_adc_read_vpack_raw(uint16_t *raw_out);
```

---

### 3.6 `board_i2c`

| Item | Detail |
|---|---|
| Responsibility | Configure I2C2 (PA9/PA10), provide byte-level I2C read/write |
| Owned state | None |
| Public API | `board_i2c_init(void)`, `board_i2c_write(addr, reg, data, len)`, `board_i2c_read(addr, reg, buf, len)` |
| Must not do | ISL28022 register logic; that is in the ISL28022 driver |
| Failure handling | Return `I2C_ERR_NACK` or `I2C_ERR_TIMEOUT`; caller handles retries |

```c
void board_i2c_init(void);
I2CResult board_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len);
I2CResult board_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);
```

---

### 3.7 `board_spi`

| Item | Detail |
|---|---|
| Responsibility | Configure SPI1 (PA5/PA6/PA7), Mode 3; provide blocking transfer; manage CS lines |
| Owned state | Current active CS (only one at a time) |
| Public API | `board_spi_init(void)`, `board_spi_select(chain)`, `board_spi_deselect(chain)`, `board_spi_transfer(tx, rx, len)` |
| Must not do | LTC6812/LTC6820 framing or PEC; that is in `isospi.c` |
| Failure handling | Return error on timeout; both CS deasserted on error |

```c
typedef enum { SPI_CHAIN_CELL, SPI_CHAIN_TEMP } SpiChain;

void board_spi_init(void);
void board_spi_select(SpiChain chain);    // Assert CS for chain; assert both deasserted first
void board_spi_deselect(SpiChain chain);  // Deassert CS
SpiResult board_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
```

---

### 3.8 `board_uart`

| Item | Detail |
|---|---|
| Responsibility | Configure USART2 (PA2/PA3), DMA or interrupt-driven TX/RX ring buffers |
| Owned state | TX/RX ring buffers |
| Public API | `board_uart_init(uint32_t baud)`, `board_uart_write(buf, len)`, `board_uart_read_byte(byte_out)` |
| Must not do | Protocol framing; that is in `bms_protocol` |

```c
void board_uart_init(uint32_t baud);
void board_uart_write(const uint8_t *buf, uint16_t len);
bool board_uart_read_byte(uint8_t *byte_out);
uint16_t board_uart_rx_available(void);
```

---

### 3.9 `board_can`

| Item | Detail |
|---|---|
| Responsibility | Configure bxCAN (PA11/PA12); TX/RX queues; filter setup |
| Owned state | TX/RX message queues |
| Public API | `board_can_init(uint32_t bitrate)`, `board_can_transmit(id, data, dlc)`, `board_can_receive(msg_out)` |
| Must not do | BMS CAN message construction; that is in `bms_can` |

```c
void board_can_init(uint32_t bitrate_bps);
CanResult board_can_transmit(uint32_t id, const uint8_t *data, uint8_t dlc);
bool board_can_receive(CanMsg *msg_out);
```

---

### 3.10 `isospi` (isoSPI Transport)

| Item | Detail |
|---|---|
| Responsibility | LTC6820 wake-up sequence; LTC6812 SPI framing (command + PEC); broadcast and read-back framing |
| Owned state | None (stateless transport) |
| Public API | Wake daisy chain, send command, write daisy chain (broadcast), read response |
| Must not do | BMS logic; LTC6812 register interpretation; CS selection (uses board_spi) |
| Chain awareness | Caller passes `SpiChain` parameter; function passes through to `board_spi_select` |

```c
// Wake: CS pulse sequence per LTC6820/LTC6812 datasheet
void isospi_wake(SpiChain chain, uint8_t num_ics);

// Send 2-byte command with PEC; no response
IsoSpiResult isospi_cmd(SpiChain chain, uint16_t cmd);

// Broadcast write: send cmd + (6 bytes + PEC) × num_ics data
IsoSpiResult isospi_write_all(SpiChain chain, uint16_t cmd,
                               const uint8_t *data, uint8_t num_ics);

// Read: send cmd; receive (6 bytes + PEC) × num_ics; verify PEC per device
IsoSpiResult isospi_read_all(SpiChain chain, uint16_t cmd,
                              uint8_t *data, uint8_t num_ics, uint8_t *pec_ok_per_ic);

// PEC calculation (CRC-15, polynomial 0x4599)
uint16_t isospi_pec15(const uint8_t *data, uint8_t len);
```

---

### 3.11 `ltc6812`

| Item | Detail |
|---|---|
| Responsibility | LTC6812 command encoding, register read/parse, configuration write, discharge bit management |
| Owned state | Per-chain IC state (cfg register image, cell/aux raw results, PEC error counters) |
| Inputs | `SpiChain` (CELL or TEMP), cell count, configuration parameters |
| Public API | See below |
| Must not do | BMS-level fault evaluation; direct GPIO writes; anything to do with balance if `chain == CHAIN_TEMP` |
| Failure handling | Return PEC error count; caller decides fault |

```c
typedef enum { CHAIN_CELL, CHAIN_TEMP } ChainId;

// Initialize register images for a chain
void ltc6812_init(ChainId chain, uint8_t num_ics);

// Wake all ICs on chain
void ltc6812_wake(ChainId chain);

// Write CFGRA to all ICs (broadcasts ic_cfg array)
// SAFETY: If chain == CHAIN_TEMP, DCC bits in cfg must be 0; driver enforces this.
Ltc6812Result ltc6812_write_cfg(ChainId chain, const Ltc6812CfgA *cfg, uint8_t num_ics);

// Write CFGRB to all ICs
Ltc6812Result ltc6812_write_cfgb(ChainId chain, const Ltc6812CfgB *cfg, uint8_t num_ics);

// Start cell voltage conversion (ADCV); CELL chain only
Ltc6812Result ltc6812_adcv(ChainId chain, uint8_t md, bool dcp);

// Start AUX/GPIO conversion (ADAX); not used for temp sensing (TEMP chain uses ADCV on C-inputs)
Ltc6812Result ltc6812_adax(ChainId chain, uint8_t md, uint8_t chg);

// Poll for conversion complete; returns when done or timeout
Ltc6812Result ltc6812_poll_adc(ChainId chain, uint32_t timeout_ms);

// Read cell voltage registers A-E; populates raw_mv[ic][cell] array
// Returns number of ICs with PEC errors (0 = all good)
uint8_t ltc6812_read_cells(ChainId chain, uint16_t raw_mv[CELL_IC_COUNT][CELLS_PER_IC]);

// Read AUX registers A-D; populates raw_uv[ic][ch] array
uint8_t ltc6812_read_aux(ChainId chain, uint16_t raw_uv[TEMP_IC_COUNT][CELLS_PER_IC]);

// Set/clear discharge bits for CELL chain only
// Asserts chain == CHAIN_CELL; returns LTC6812_ERR_TEMP_CHAIN if violated
Ltc6812Result ltc6812_set_balance_mask(uint8_t balance_bits[CELL_IC_COUNT][CELLS_PER_IC]);
Ltc6812Result ltc6812_clear_balance_all(void);

// Set S-output bits (TEMP chain sensor bias, or GPIO for CELL chain)
Ltc6812Result ltc6812_set_s_outputs(ChainId chain, uint8_t s_bits[TEMP_IC_COUNT]);
Ltc6812Result ltc6812_clear_s_outputs(ChainId chain);
```

---

### 3.12 `isl28022`

| Item | Detail |
|---|---|
| Responsibility | Configure ISL28022; read Vbat (V_bus) and I_batt (V_shunt); convert to physical units |
| Owned state | Configuration register image; last read values and timestamps |
| Inputs | I2C address, shunt resistance (from config or compile constant) |
| Public API | See below |
| Must not do | Fault evaluation; permission decisions |

```c
typedef struct {
    int32_t vbat_mv;         // Battery-side voltage in mV
    int32_t i_batt_ma;       // Current in mA (positive = discharge)
    uint32_t timestamp_ms;
    bool valid;
} Isl28022Reading;

Isl28022Result isl28022_init(uint8_t i2c_addr, uint16_t shunt_mohm);
Isl28022Result isl28022_read(Isl28022Reading *out);
```

---

### 3.13 `vpack_adc`

| Item | Detail |
|---|---|
| Responsibility | Read PA1 ADC raw value; apply gain/offset calibration from config; return Vpack in mV |
| Owned state | Calibration constants (loaded from config) |
| Public API | See below |
| Must not do | Fault evaluation; accessing config directly (calibration passed in at init) |

```c
typedef struct {
    uint32_t vpack_mv;
    uint32_t timestamp_ms;
    bool valid;
} VpackReading;

void vpack_adc_init(uint16_t gain_x1000, int16_t offset_mv);
VpackResult vpack_adc_read(VpackReading *out);
```

---

### 3.14 `bms_measurements`

| Item | Detail |
|---|---|
| Responsibility | Orchestrate all measurement cycles; validate results; populate measurement state |
| Owned state | `BmsMeasurementState`: all cell voltages, temperatures, Vbat, Vpack, current; validity flags; timestamps |
| Inputs | Config (thresholds, settle time, masks); ltc6812/isl28022/vpack_adc drivers |
| Outputs | `BmsMeasurementState` (read by `bms_faults`, `bms_balance`, `bms_protocol`) |
| Must not do | Decide permissions; write GPIOs; interpret faults |

```c
typedef struct {
    uint16_t cell_mv[CELL_COUNT];
    bool     cell_valid[CELL_COUNT];
    uint32_t cell_timestamp_ms;
    int16_t  temp_c_x10[TEMP_COUNT];   // Temperature × 10 for integer: 250 = 25.0°C
    bool     temp_valid[TEMP_COUNT];
    uint32_t temp_timestamp_ms;
    int32_t  vbat_mv;
    bool     vbat_valid;
    uint32_t vbat_timestamp_ms;
    uint32_t vpack_mv;
    bool     vpack_valid;
    uint32_t vpack_timestamp_ms;
    int32_t  i_batt_ma;
    bool     current_valid;
    uint32_t current_timestamp_ms;
} BmsMeasurementState;

void bms_measurements_init(void);
void bms_measurements_tick(void);   // Called each main loop tick; runs all measurement cycles
const BmsMeasurementState *bms_measurements_get(void);

// TEMP measurement sequence: bias-on → settle → convert → read → bias-off (always)
MeasResult bms_measurements_run_temp_cycle(void);
```

**Temperature measurement error path must always call `ltc6812_clear_s_outputs(CHAIN_TEMP)`.**

**Tests:**
- Mock ltc6812/isl28022 drivers; inject PEC errors; verify measurement invalidation
- Inject stale timestamp; verify VALIDITY_STALE triggers
- Verify S outputs always cleared in both success and error paths

---

### 3.15 `bms_faults`

| Item | Detail |
|---|---|
| Responsibility | Evaluate measurement state against config thresholds; maintain active/latched fault bitmaps |
| Owned state | `active_faults` (uint64_t), `latched_faults` (uint64_t) |
| Inputs | `BmsMeasurementState`, `BmsConfig` |
| Outputs | Fault bitmaps (read by `bms_outputs`, `bms_state`, `bms_protocol`) |
| Must not do | Write GPIOs; make permission decisions; clear latched faults without caller authorization |

```c
void bms_faults_init(void);
void bms_faults_evaluate(const BmsMeasurementState *meas, const BmsConfig *cfg);
uint64_t bms_faults_get_active(void);
uint64_t bms_faults_get_latched(void);
bool bms_faults_is_set(uint8_t fault_bit);
void bms_faults_clear_latched(uint64_t mask);  // Only clears if active condition is resolved
void bms_faults_force_set(uint8_t fault_bit);  // For driver-detected faults (e.g., TEMP chain violation)
```

**Tests:**
- Each fault bit: inject condition in mock measurement → verify bit set
- Latching: clear condition → verify latched bit remains → explicit clear → verify cleared
- Threshold boundary: cell at exactly OV threshold → check edge behaviour

---

### 3.16 `bms_outputs`

| Item | Detail |
|---|---|
| Responsibility | Decide permission assertions based on fault bitmap and requested state; drive board_outputs |
| Owned state | Current desired state from state machine; current output state |
| Inputs | Fault bitmaps from `bms_faults`; desired state from `bms_state` |
| Must not do | Override permission decisions based on protocol commands; bypass fault gates |
| Rules | Only this module calls `board_output_set` for permission outputs |

```c
typedef struct {
    bool want_master_ok;
    bool want_discharge;
    bool want_charge;
    bool want_charger_safety;
} BmsPermissionRequest;

void bms_outputs_init(void);
void bms_outputs_apply(const BmsPermissionRequest *req, uint64_t active_faults, uint64_t latched_faults);
void bms_outputs_deassert_all(void);   // Called on fatal/shutdown
bool bms_outputs_get(OutputId id);
```

**Tests:**
- Inject fault → verify output deasserted despite request
- No fault + valid request → verify output asserted
- Fatal: all outputs deasserted regardless of request

---

### 3.17 `bms_state`

| Item | Detail |
|---|---|
| Responsibility | State machine: BOOT → IDLE → PRECHARGING → CLOSING_MAIN → DISCHARGING → CHARGING → BALANCING → FAULT → SHUTDOWN |
| Owned state | Current state enum, precharge timer, transition history |
| Inputs | Fault bitmaps, measurement state, protocol commands (state change requests) |
| Outputs | `BmsPermissionRequest` to `bms_outputs`; state published to protocol |
| Must not do | Write GPIOs directly; access hardware drivers; interpret raw measurements |

```c
typedef enum {
    BMS_STATE_BOOT,
    BMS_STATE_IDLE,
    BMS_STATE_PRECHARGING,
    BMS_STATE_CLOSING_MAIN,
    BMS_STATE_DISCHARGING,
    BMS_STATE_CHARGING,
    BMS_STATE_BALANCING,
    BMS_STATE_FAULT,
    BMS_STATE_CONFIG_ERROR,
    BMS_STATE_SHUTDOWN,
} BmsState;

void bms_state_init(void);
void bms_state_tick(void);
BmsState bms_state_get(void);
void bms_state_request_transition(BmsState requested);  // From protocol; validated by state machine
```

---

### 3.18 `bms_balance`

| Item | Detail |
|---|---|
| Responsibility | Compute per-cell balance decisions; write DCC bits to CELL chain only |
| Owned state | Current balance mask |
| Inputs | Measurement state, fault bitmaps, config (thresholds, allowed mask), state |
| Must not do | Write to TEMP chain; override fault gates; initiate balancing when cells invalid |

```c
void bms_balance_init(void);
void bms_balance_tick(void);   // Evaluates and applies/clears balance mask each cycle
void bms_balance_disable_all(void);  // Immediate clear of all DCC bits
```

Balance algorithm (simple threshold-based v1):
1. If any gating condition (see §7 of safety model): clear all, return
2. For each cell in `balance_allowed_mask`: if `cell_mv[i] > balance_target_mv + balance_hysteresis_mv`: set DCC bit
3. Apply computed mask via `ltc6812_set_balance_mask()`

---

### 3.19 `bms_config`

| Item | Detail |
|---|---|
| Responsibility | Load, validate, and store BMS configuration; provide config to other modules |
| Owned state | Active config struct; loaded/default flag |
| Public API | Load from flash, validate, get pointer, write+store (after full validation), reset to defaults |
| Must not do | Apply config changes silently; accept partial configs; allow unsaved dirty state without explicit flag |

```c
typedef enum { CFG_LOAD_OK, CFG_LOAD_INVALID, CFG_LOAD_DEFAULT } ConfigLoadResult;

ConfigLoadResult bms_config_load(void);
const BmsConfig  *bms_config_get(void);
ConfigResult     bms_config_validate(const BmsConfig *cfg);
ConfigResult     bms_config_store(const BmsConfig *cfg);  // Validates first; then writes to flash
void             bms_config_load_defaults(BmsConfig *cfg_out);
```

---

### 3.20 `bms_protocol`

| Item | Detail |
|---|---|
| Responsibility | Packet framing, CRC, request dispatch, response serialization over UART |
| Owned state | RX ring buffer state; current in-progress packet |
| Inputs | UART byte stream; measurement/fault/state data (via getters) |
| Outputs | UART byte stream; dispatched commands to state machine / config |
| Must not do | BMS logic; permission decisions; direct hardware access |

```c
void bms_protocol_init(void);
void bms_protocol_tick(void);  // Called from main loop; processes pending bytes, sends responses
```

Internally routes packet IDs to handlers. See `docs/04_protocol_contract.md`.

---

### 3.21 `bms_diagnostics`

| Item | Detail |
|---|---|
| Responsibility | Diagnostic ring buffer; PEC error counters; open-wire result storage; test result reporting |
| Owned state | Ring buffer of `DiagEvent` entries; open-wire result per cell; LTC6812 self-test results |
| Public API | Push event, get summary, clear buffer |

```c
void bms_diagnostics_init(void);
void bms_diagnostics_push(DiagEventType type, uint32_t data);
DiagSummary bms_diagnostics_get_summary(void);
void bms_diagnostics_run_openwire(void);   // Triggers open-wire diagnostic cycle
```

---

### 3.22 `bms_can`

| Item | Detail |
|---|---|
| Responsibility | Encode and transmit CAN telemetry frames; receive and handle CAN commands |
| Owned state | CAN TX schedule; last TX timestamps per message type |
| Must not do | Safety decisions; permission writes; direct hardware |

```c
void bms_can_init(void);
void bms_can_tick(void);   // Services CAN TX schedule; processes RX queue
```

CAN message IDs and rates: **OPEN QUESTION** — define CAN base ID, message layout, and rate for each telemetry type (cell voltages, temperatures, status).

---

### 3.23 `bms_update_interface`

| Item | Detail |
|---|---|
| Responsibility | Handle `ENTER_BOOTLOADER` protocol command; write boot flag to retained SRAM or flash; trigger reset |
| Owned state | Boot flag location |
| Must not do | Direct flash programming; firmware package validation (that is bootloader's job) |

```c
void bms_update_init(void);
UpdateResult bms_update_request_bootloader_entry(void);   // Sets flag + triggers reset
bool bms_update_is_bootloader_requested(void);            // Read at boot to decide boot path
```

---

## 4. Module Interaction Rules

```
Measurement data flows ONE direction:
  bms_measurements → bms_faults → bms_outputs
                   → bms_balance
                   → bms_protocol (read-only)

Config flows:
  bms_config ──────────────────► all modules (read-only pointer)
  bms_protocol ──validate+write► bms_config

State machine:
  bms_state ──request──► bms_outputs
  bms_state reads faults from bms_faults (never writes)

GPIO:
  board_outputs ◄──────────────── bms_outputs ONLY
```

No module except `bms_outputs` calls `board_output_set()`. This is enforced by not including `board_outputs.h` in any other BMS module header.

---

## 5. Build System

- CMake with arm-none-eabi-gcc toolchain file
- Separate targets: `bms_firmware.elf`, `bms_tests` (native host build)
- `bms_tests` compiles all `bms/` and `drivers/` with mock BSP substitutes
- `firmware_package` target: builds ELF, extracts binary, generates firmware package header
- All warnings treated as errors (`-Wall -Wextra -Werror`)
- C11 standard
