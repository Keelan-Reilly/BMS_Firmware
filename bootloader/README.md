# Bootloader

Separate STM32F303 firmware image occupying flash region `[0x08000000, 0x08007FFF]` (32 KB, PROVISIONAL).

The bootloader is independent of the BMS application — it has its own vector table, startup code, and build. It shares only `bms_constants.h` for flash map constants.

---

## Responsibilities

1. **Boot decision:** Jump to application or stay in bootloader mode (see boot decision flow below).
2. **Identity:** Respond to `GET_CAPABILITIES` and `GET_BOOT_INFO` over UART.
3. **Package validation:** Validate firmware package header before any flash erase.
4. **Flash programming:** Erase app region and write chunks.
5. **Post-transfer verification:** CRC32 the written image and compare to package header.
6. **Config preservation:** Never touch config flash regions.

---

## Folder Structure

```
bootloader/
  src/
    main.c                 Startup: clock init, UART init, boot decision, main loop
    bl_uart.c/h            UART framing: same protocol as BMS app (shared framing logic)
    bl_protocol.c/h        Handles GET_CAPABILITIES, GET_BOOT_INFO, BOOT_UPDATE_* packets
    bl_flash.c/h           Flash erase/write/verify (STM32F303 HAL-free, direct register)
    bl_validate.c/h        Package header validation, vector table checks
    bl_jump.c/h            Safe jump to application (disable IRQs, set VTOR, MSP, branch)
  include/
    bl_config.h            Bootloader version, flash map constants (shared with firmware)
  linker/
    stm32f303_bl.ld        Linker script: FLASH ORIGIN=0x08000000, LENGTH=32K
  CMakeLists.txt
```

---

## Identity / Capabilities

When in bootloader mode, `GET_CAPABILITIES` returns:
- `firmware_type = 0x0002` (BOOTLOADER)
- `firmware_version` = bootloader version (separate from app version)
- `hw_profile_id` = same as app (compiled in)
- `protocol_version` = same protocol version as app
- `config_schema_version` = schema version the bootloader knows about (for tool guidance)
- `feature_flags.FEAT_BOOTLOADER = 1`

`GET_BOOT_INFO` returns flash map addresses and sizes (all compile-time constants).

---

## Package Validation

Validation order (see `protocol/firmware_package.yaml` for full spec):
1. `pkg_magic == 0xBF00BF00`
2. `pkg_version <= BL_MAX_PKG_VERSION`
3. `hw_profile_id == HW_PROFILE_ID`
4. `target_mcu_id == DBGMCU_IDCODE` (read at runtime from `0xE0042000`)
5. `image_type == 0x01`
6. `app_start_addr == APP_START_ADDR`
7. `0 < app_size <= APP_REGION_SIZE`
8. Header CRC32 matches
9. Bootloader version >= `min_bootloader_version` in package

On any failure: return `ERR_PACKAGE_INVALID` or `ERR_WRONG_TARGET`; do NOT erase flash.

---

## Flash Safety Checks

Before erasing:
- Verify erase range is entirely within `[APP_START_ADDR, APP_START_ADDR + APP_REGION_SIZE]`
- Never erase bootloader region (`< APP_START_ADDR`)
- Never erase config region (`>= CONFIG_A_START_ADDR`)

Flash write:
- Write in 16-bit half-words (STM32F303 minimum write unit)
- Verify each written word reads back correctly
- On write failure: set error, abort, respond with `ERR_FLASH_FAIL`

---

## Boot Decision Flow

```c
void bootloader_main(void) {
    // 1. Read boot flag from RTC backup register
    if (RTC->BKP0R == BL_ENTRY_FLAG) {
        RTC->BKP0R = 0;  // Clear flag
        goto stay_in_bootloader;
    }

    // 2. Check for incomplete update
    if (update_staging_has_incomplete_marker()) {
        goto stay_in_bootloader;
    }

    // 3. Validate application image
    uint32_t sp = *(volatile uint32_t*)APP_START_ADDR;
    uint32_t rv = *(volatile uint32_t*)(APP_START_ADDR + 4);
    if (!is_valid_sp(sp) || !is_valid_rv(rv)) {
        goto stay_in_bootloader;
    }
    if (!verify_app_crc()) {  // CRC stored in dedicated metadata or computed from image
        goto stay_in_bootloader;
    }

    // 4. Jump to application
    bl_jump_to_app(APP_START_ADDR);

stay_in_bootloader:
    bl_protocol_run();  // Loop forever; handle packets
}
```

---

## Update States

| State | Meaning |
|---|---|
| `BL_STATE_IDLE` | Awaiting `BOOT_UPDATE_BEGIN` |
| `BL_STATE_HEADER_ACCEPTED` | Header validated; awaiting chunk 0 |
| `BL_STATE_RECEIVING` | Chunks being received and flashed |
| `BL_STATE_VERIFYING` | All chunks received; CRC check in progress |
| `BL_STATE_COMPLETE` | CRC passed; new image valid; ready to boot |
| `BL_STATE_ERROR` | Flash or CRC error; awaiting ABORT or BEGIN retry |

State is in-memory only (not persisted to flash). Power loss resets to IDLE.

---

## Config Preservation

The bootloader never reads, writes, or erases the config region. The app region erase uses explicit start/end addresses that exclude `CONFIG_A_START_ADDR` and above. There is a compile-time assert in `bl_flash.c`:

```c
_Static_assert(APP_START_ADDR + APP_REGION_SIZE <= CONFIG_A_START_ADDR,
               "App region must not overlap config region");
```

---

## Failure Handling

| Failure | Response |
|---|---|
| Package header validation fail | ERR_PACKAGE_INVALID; remain IDLE |
| Wrong hw_profile_id | ERR_WRONG_TARGET; remain IDLE |
| Flash erase failure | ERR_FLASH_FAIL; enter ERROR state |
| Flash write failure | ERR_FLASH_FAIL; enter ERROR state |
| CRC mismatch after transfer | ERR_PACKAGE_INVALID; app region in indeterminate state; must retry BEGIN |
| Power loss mid-write | CRC will fail at next boot; bootloader stays in bootloader mode |
| Wrong chunk sequence | ERR_SEQUENCE; expect host to retry from BEGIN |

---

## ST-Link Relationship

The bootloader coexists with ST-Link development flashing. During ST-Link flash:
- The full flash (including bootloader region) can be overwritten
- Development combined image starts at 0x08000000 (includes bootloader)
- The bootloader region is only written when explicitly addressed by the programmer

ST-Link flashing does not use the bootloader protocol. They are independent paths.

---

## Tests

See `docs/08_validation_plan.md §6`. Bootloader tests include:
- Valid image → verify jump attempted (mock jump function records call)
- Bad SP / bad reset vector → stay in bootloader
- CRC fail → stay in bootloader
- Boot flag set → stay in bootloader, clear flag
- Config region preserved after update
- Wrong chunk sequence → ERR_SEQUENCE
- Power-loss simulation (partial write) → CRC fails → stays in bootloader

All tests are host-compiled with mock flash (array of `uint8_t`). No hardware required for unit tests.
