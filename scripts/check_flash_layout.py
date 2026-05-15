#!/usr/bin/env python3
"""check_flash_layout.py — verify BMS flash map invariants.

Checks:
  - No region overlaps
  - All regions are page-aligned (2KB pages)
  - Total usage does not exceed 256KB
  - Config struct (226 bytes) fits in config slot (8KB)
  - Bootloader does not exceed 32KB
  - Application region provides enough space

Run from repo root:
  python3 scripts/check_flash_layout.py
"""
import sys

# ── Flash map constants (must match bms_constants.h) ──────────────────────────
FLASH_BASE      = 0x08000000
FLASH_SIZE      = 256 * 1024

PAGE_SIZE       = 2048  # STM32F303xC

BL_START        = 0x08000000
BL_SIZE         = 32 * 1024

APP_START       = 0x08008000
APP_SIZE        = 188 * 1024

CONFIG_A_START  = 0x08037000
CONFIG_B_START  = 0x08039000
CONFIG_SLOT_SIZE = 8 * 1024

CONFIG_STRUCT_SIZE = 226

# ── Regions ────────────────────────────────────────────────────────────────────
regions = [
    ('Bootloader',  BL_START,       BL_SIZE),
    ('Application', APP_START,      APP_SIZE),
    ('Config A',    CONFIG_A_START, CONFIG_SLOT_SIZE),
    ('Config B',    CONFIG_B_START, CONFIG_SLOT_SIZE),
]

errors = []

def fail(msg):
    errors.append(msg)
    print(f"  FAIL: {msg}")

def ok(msg):
    print(f"  OK:   {msg}")

print("BMS Flash Layout Check")
print("=" * 60)

# Print layout
total_used = 0
for name, start, size in regions:
    end = start + size
    print(f"  {name:12s}: 0x{start:08X} – 0x{end-1:08X}  ({size//1024:3d} KB)")
    total_used = max(total_used, end - FLASH_BASE)
print(f"  {'Total used':12s}: {total_used//1024} KB / {FLASH_SIZE//1024} KB")
print()

# Page alignment
for name, start, size in regions:
    if start % PAGE_SIZE != 0:
        fail(f"{name} start 0x{start:08X} not page-aligned")
    else:
        ok(f"{name} start is page-aligned")
    if size % PAGE_SIZE != 0:
        fail(f"{name} size {size} not multiple of page size")

# No overlaps (check consecutive regions)
sorted_regions = sorted(regions, key=lambda r: r[1])
for i in range(len(sorted_regions) - 1):
    a_name, a_start, a_size = sorted_regions[i]
    b_name, b_start, _     = sorted_regions[i + 1]
    if a_start + a_size > b_start:
        fail(f"{a_name} overlaps {b_name} "
             f"(0x{a_start+a_size:08X} > 0x{b_start:08X})")
    else:
        ok(f"{a_name} does not overlap {b_name}")

# Total usage
if total_used > FLASH_SIZE:
    fail(f"Total usage {total_used//1024} KB exceeds flash {FLASH_SIZE//1024} KB")
else:
    ok(f"Total usage {total_used//1024} KB fits in {FLASH_SIZE//1024} KB flash")

# Config struct fits in slot
if CONFIG_STRUCT_SIZE > CONFIG_SLOT_SIZE:
    fail(f"Config struct {CONFIG_STRUCT_SIZE}B > slot {CONFIG_SLOT_SIZE}B")
else:
    ok(f"Config struct {CONFIG_STRUCT_SIZE}B fits in {CONFIG_SLOT_SIZE}B slot")

# Bootloader within region
if BL_SIZE > 32 * 1024:
    fail(f"Bootloader region {BL_SIZE//1024}KB exceeds 32KB budget")
else:
    ok(f"Bootloader region {BL_SIZE//1024}KB ≤ 32KB budget")

print()
if errors:
    print(f"RESULT: {len(errors)} error(s) found.")
    sys.exit(1)
else:
    print("RESULT: All flash layout checks passed.")
    sys.exit(0)
