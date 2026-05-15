"""test_package_parser.py — firmware package header parse and validate tests."""
import struct
import pytest
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from tool.src.update.package_parser import (
    parse_header, validate_header, PackageValidationError,
    PKG_MAGIC, APP_START_ADDR, APP_REGION_SIZE, STM32F303VC_DEV_ID
)
from tool.src.protocol.crc import crc32_iso_hdlc


def make_valid_header_bytes() -> bytes:
    """Build a valid 64-byte header blob."""
    hdr = bytearray(64)
    struct.pack_into('<I', hdr, 0,  PKG_MAGIC)
    struct.pack_into('<H', hdr, 4,  1)                    # pkg_version
    struct.pack_into('<H', hdr, 6,  0x0001)               # hw_profile_id
    struct.pack_into('<I', hdr, 8,  STM32F303VC_DEV_ID)   # target_mcu_id
    hdr[12] = 0x01                                         # image_type = APP
    struct.pack_into('<I', hdr, 16, APP_START_ADDR)
    struct.pack_into('<I', hdr, 20, 4096)                 # app_size
    struct.pack_into('<I', hdr, 24, 0xDEAD1234)           # app_crc32 (payload not checked here)
    hdr[28] = 0; hdr[29] = 1; hdr[30] = 0                 # fw_version 0.1.0
    hdr[31] = 0; hdr[32] = 0; hdr[33] = 0                 # min_bl_version 0.0.0
    struct.pack_into('<H', hdr, 34, 1)                    # required_config_schema
    # Compute header CRC over bytes 0..37 (0x00..0x25)
    crc = crc32_iso_hdlc(bytes(hdr[:0x26]))
    struct.pack_into('<I', hdr, 0x26, crc)
    return bytes(hdr)


def test_valid_header_passes():
    raw = make_valid_header_bytes()
    hdr = parse_header(raw)
    validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=raw)

def test_bad_magic_raises():
    raw = bytearray(make_valid_header_bytes())
    struct.pack_into('<I', raw, 0, 0xDEADBEEF)
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="Bad magic"):
        validate_header(hdr, raw_header=bytes(raw))

def test_wrong_hw_profile_raises():
    raw = bytearray(make_valid_header_bytes())
    struct.pack_into('<H', raw, 6, 0x9999)
    crc = crc32_iso_hdlc(bytes(raw[:0x26]))
    struct.pack_into('<I', raw, 0x26, crc)
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="hw_profile_id"):
        validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=bytes(raw))

def test_wrong_mcu_id_raises():
    raw = bytearray(make_valid_header_bytes())
    struct.pack_into('<I', raw, 8, 0x999)
    crc = crc32_iso_hdlc(bytes(raw[:0x26]))
    struct.pack_into('<I', raw, 0x26, crc)
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="MCU dev_id"):
        validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=bytes(raw))

def test_bad_header_crc_raises():
    raw = bytearray(make_valid_header_bytes())
    raw[0x26] ^= 0xFF  # corrupt CRC
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="CRC"):
        validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=bytes(raw))

def test_oversized_app_raises():
    raw = bytearray(make_valid_header_bytes())
    struct.pack_into('<I', raw, 20, APP_REGION_SIZE + 1)
    crc = crc32_iso_hdlc(bytes(raw[:0x26]))
    struct.pack_into('<I', raw, 0x26, crc)
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="app_size"):
        validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=bytes(raw))

def test_wrong_app_addr_raises():
    raw = bytearray(make_valid_header_bytes())
    struct.pack_into('<I', raw, 16, 0x08000000)  # bootloader addr
    crc = crc32_iso_hdlc(bytes(raw[:0x26]))
    struct.pack_into('<I', raw, 0x26, crc)
    hdr = parse_header(bytes(raw))
    with pytest.raises(PackageValidationError, match="app_start_addr"):
        validate_header(hdr, mcu_dev_id=STM32F303VC_DEV_ID, raw_header=bytes(raw))
