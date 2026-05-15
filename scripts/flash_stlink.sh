#!/usr/bin/env bash
# flash_stlink.sh — flash firmware to STM32F303VC via ST-Link / STM32_Programmer_CLI.
#
# Dependencies:
#   STM32_Programmer_CLI (STM32CubeProgrammer, on PATH or set STM32_PROGRAMMER env var)
#
# Usage:
#   ./scripts/flash_stlink.sh [path/to/firmware.bin]
#   ./scripts/flash_stlink.sh [path/to/firmware.hex]
set -euo pipefail

PROGRAMMER="${STM32_PROGRAMMER:-STM32_Programmer_CLI}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIRMWARE="${1:-${REPO_ROOT}/build_firmware/firmware.hex}"

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Error: firmware file not found: ${FIRMWARE}" >&2
    echo "Run ./scripts/build_firmware.sh first." >&2
    exit 1
fi

echo "==> Flashing ${FIRMWARE}"
echo "    Programmer: ${PROGRAMMER}"
echo "    Target: STM32F303VC via SWD"

# Detect and flash
"${PROGRAMMER}" -c port=SWD freq=4000 -d "${FIRMWARE}" -v -rst

echo ""
echo "==> Flash complete. Device should be running BMS firmware."
