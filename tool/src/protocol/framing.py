"""framing.py — BMS protocol frame encode/decode.

Frame format (little-endian):
  SOF[0]      0xAA
  SOF[1]      0x55
  PKT_ID      uint16 LE
  FLAGS       uint8  (bit0=IS_RESPONSE, bit1=IS_ERROR)
  SEQ         uint8
  PAYLOAD_LEN uint16 LE
  PAYLOAD     bytes (0..N)
  FRAME_CRC   uint16 BE (CRC-16/CCITT-FALSE over SOF..PAYLOAD)
"""
import struct
from .crc import crc16_ccitt

SOF_BYTE_0 = 0xAA
SOF_BYTE_1 = 0x55
FRAME_OVERHEAD = 10  # SOF(2)+PKT_ID(2)+FLAGS(1)+SEQ(1)+LEN(2)+CRC(2)

FLAG_IS_RESPONSE = 0x01
FLAG_IS_ERROR    = 0x02


class FrameError(Exception):
    pass


def encode_frame(pkt_id: int, payload: bytes, seq: int = 0,
                 is_response: bool = False, is_error: bool = False) -> bytes:
    """Encode a protocol frame."""
    flags = 0
    if is_response: flags |= FLAG_IS_RESPONSE
    if is_error:    flags |= FLAG_IS_ERROR
    header = struct.pack('<BBHBBH',
                         SOF_BYTE_0, SOF_BYTE_1,
                         pkt_id, flags, seq, len(payload))
    body = header + payload
    crc = crc16_ccitt(body)
    return body + struct.pack('>H', crc)


def decode_frame(data: bytes) -> dict:
    """Decode a protocol frame. Returns dict with pkt_id, flags, seq, payload.
    Raises FrameError on bad SOF or CRC.
    """
    if len(data) < FRAME_OVERHEAD:
        raise FrameError(f"Frame too short: {len(data)} bytes")
    if data[0] != SOF_BYTE_0 or data[1] != SOF_BYTE_1:
        raise FrameError(f"Bad SOF: {data[0]:02X} {data[1]:02X}")
    pkt_id, flags, seq, payload_len = struct.unpack_from('<HBBH', data, 2)
    if len(data) < FRAME_OVERHEAD + payload_len:
        raise FrameError("Frame truncated")
    payload = data[8:8 + payload_len]
    recv_crc = struct.unpack_from('>H', data, 8 + payload_len)[0]
    calc_crc = crc16_ccitt(data[:8 + payload_len])
    if recv_crc != calc_crc:
        raise FrameError(f"CRC mismatch: got {recv_crc:04X}, expected {calc_crc:04X}")
    return {
        'pkt_id':  pkt_id,
        'flags':   flags,
        'seq':     seq,
        'payload': payload,
        'is_response': bool(flags & FLAG_IS_RESPONSE),
        'is_error':    bool(flags & FLAG_IS_ERROR),
    }


class FrameDecoder:
    """Stateful byte-by-byte frame decoder for streaming UART data."""

    def __init__(self):
        self._buf = bytearray()
        self._state = 'SOF0'
        self._payload_len = 0

    def feed(self, data: bytes) -> list:
        """Feed bytes; returns list of decoded frames (as dicts)."""
        frames = []
        for byte in data:
            frames.extend(self._consume(byte))
        return frames

    def _consume(self, b: int) -> list:
        self._buf.append(b)
        if self._state == 'SOF0':
            if b == SOF_BYTE_0:
                self._state = 'SOF1'
            else:
                self._buf.clear()
        elif self._state == 'SOF1':
            if b == SOF_BYTE_1:
                self._state = 'HEADER'
            else:
                self._buf = bytearray([b]) if b == SOF_BYTE_0 else bytearray()
                self._state = 'SOF1' if b == SOF_BYTE_0 else 'SOF0'
        elif self._state == 'HEADER':
            if len(self._buf) == 8:  # SOF(2)+ID(2)+FLAGS(1)+SEQ(1)+LEN(2)
                self._payload_len = struct.unpack_from('<H', self._buf, 6)[0]
                self._state = 'PAYLOAD' if self._payload_len else 'CRC'
        elif self._state == 'PAYLOAD':
            if len(self._buf) == 8 + self._payload_len:
                self._state = 'CRC'
        elif self._state == 'CRC':
            if len(self._buf) == 8 + self._payload_len + 2:
                try:
                    frame = decode_frame(bytes(self._buf))
                    self._buf.clear()
                    self._state = 'SOF0'
                    return [frame]
                except FrameError:
                    self._buf.clear()
                    self._state = 'SOF0'
        return []
