#!/usr/bin/env bash
# build_firmware.sh — build BMS firmware for STM32F303VC.
#
# Dependencies:
#   arm-none-eabi-gcc (GNU Arm Embedded Toolchain)
#   cmake >= 3.20
#   ninja or make
#
# Usage:
#   ./scripts/build_firmware.sh [debug|release]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${1:-release}"
BUILD_DIR="${REPO_ROOT}/build_firmware"

case "${BUILD_TYPE}" in
  release|Release)
    CMAKE_BUILD_TYPE="Release"
    ;;
  debug|Debug)
    CMAKE_BUILD_TYPE="Debug"
    ;;
  *)
    echo "error: build type must be 'debug' or 'release'" >&2
    exit 1
    ;;
esac

echo "==> BMS Firmware Build (${CMAKE_BUILD_TYPE})"
echo "    Toolchain: $(arm-none-eabi-gcc --version | head -1)"

# Configure
cmake -B "${BUILD_DIR}" \
      -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/firmware/cmake/arm_none_eabi.cmake" \
      -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
      -DBMS_BUILD_TESTS=OFF \
      -G Ninja \
      -S "${REPO_ROOT}/firmware"

# Build
cmake --build "${BUILD_DIR}" --target bms_firmware.elf

echo ""
echo "==> Build artifacts:"
ls -lh "${BUILD_DIR}/firmware.bin" "${BUILD_DIR}/firmware.hex" "${BUILD_DIR}/bms_firmware.elf" 2>/dev/null || true
echo ""
echo "==> Flash layout:"
python3 "${REPO_ROOT}/scripts/check_flash_layout.py"
