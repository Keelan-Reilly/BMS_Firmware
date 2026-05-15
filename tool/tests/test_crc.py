"""test_crc.py — CRC-16 and CRC-32 tests."""
import pytest
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from tool.src.protocol.crc import crc16_ccitt, crc32_iso_hdlc


def test_crc16_hello():
    assert crc16_ccitt(b'Hello') == 0xDADA

def test_crc16_check_vector():
    assert crc16_ccitt(b'123456789') == 0x29B1

def test_crc16_empty():
    assert crc16_ccitt(b'') == 0xFFFF

def test_crc32_empty():
    assert crc32_iso_hdlc(b'') == 0x00000000

def test_crc32_check_vector():
    # CRC-32/ISO-HDLC of "123456789" = 0xCBF43926
    assert crc32_iso_hdlc(b'123456789') == 0xCBF43926

def test_crc16_consistency():
    data = bytes(range(256))
    assert crc16_ccitt(data) == crc16_ccitt(data)

def test_crc32_changes_with_data():
    a = crc32_iso_hdlc(b'\x00')
    b = crc32_iso_hdlc(b'\x01')
    assert a != b
