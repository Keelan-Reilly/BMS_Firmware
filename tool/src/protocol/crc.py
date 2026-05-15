"""crc.py — CRC-16/CCITT-FALSE for BMS protocol framing.

Polynomial: 0x1021, Initial value: 0xFFFF, No final XOR, No reflection.
"""


def crc16_ccitt(data: bytes) -> int:
    """Compute CRC-16/CCITT-FALSE over data bytes."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def crc32_iso_hdlc(data: bytes) -> int:
    """CRC-32/ISO-HDLC: poly=0xEDB88320 (reflected), init=0xFFFFFFFF, final XOR=0xFFFFFFFF.
    Used for config CRC and firmware package CRC.
    """
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xEDB88320) if (crc & 1) else (crc >> 1)
    return crc ^ 0xFFFFFFFF
