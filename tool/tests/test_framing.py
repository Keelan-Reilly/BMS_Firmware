"""test_framing.py — protocol frame encode/decode tests."""
import pytest
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from tool.src.protocol.framing import encode_frame, decode_frame, FrameDecoder, FrameError
from tool.src.protocol.packet_defs import PKT_GET_CAPABILITIES


def test_encode_decode_empty_payload():
    frame = encode_frame(PKT_GET_CAPABILITIES, b'')
    result = decode_frame(frame)
    assert result['pkt_id'] == PKT_GET_CAPABILITIES
    assert result['payload'] == b''
    assert not result['is_response']
    assert not result['is_error']

def test_encode_decode_with_payload():
    payload = bytes(range(10))
    frame = encode_frame(0x0101, payload, seq=42)
    result = decode_frame(frame)
    assert result['pkt_id'] == 0x0101
    assert result['payload'] == payload
    assert result['seq'] == 42

def test_response_flags():
    frame = encode_frame(0x0001, b'', is_response=True)
    result = decode_frame(frame)
    assert result['is_response']
    assert not result['is_error']

def test_error_flags():
    frame = encode_frame(0x0001, b'\x01', is_response=True, is_error=True)
    result = decode_frame(frame)
    assert result['is_error']

def test_bad_crc_raises():
    frame = bytearray(encode_frame(0x0001, b'test'))
    frame[-1] ^= 0xFF  # corrupt last CRC byte
    with pytest.raises(FrameError):
        decode_frame(bytes(frame))

def test_bad_sof_raises():
    frame = bytearray(encode_frame(0x0001, b''))
    frame[0] = 0x00
    with pytest.raises(FrameError):
        decode_frame(bytes(frame))

def test_streaming_decoder_reassembles():
    frame = encode_frame(PKT_GET_CAPABILITIES, b'hello')
    decoder = FrameDecoder()
    frames = []
    for byte in frame:
        frames.extend(decoder.feed(bytes([byte])))
    assert len(frames) == 1
    assert frames[0]['payload'] == b'hello'

def test_streaming_decoder_multiple_frames():
    f1 = encode_frame(0x0001, b'a')
    f2 = encode_frame(0x0002, b'bb')
    decoder = FrameDecoder()
    frames = decoder.feed(f1 + f2)
    assert len(frames) == 2
    assert frames[0]['pkt_id'] == 0x0001
    assert frames[1]['pkt_id'] == 0x0002

def test_frame_with_noise_prefix():
    noise = bytes([0x00, 0xFF, 0x12])
    frame = encode_frame(0x0001, b'x')
    decoder = FrameDecoder()
    frames = decoder.feed(noise + frame)
    assert len(frames) == 1
