"""fake_target.py — simulated BMS device for tool development and CI tests.

Implements the full BMS request/response protocol over a serial-like interface.
Can run as:
  - Subprocess on a virtual serial port pair (socat for Linux/macOS)
  - In-process mock for unit tests (use FakeTargetInProcess)

Usage (subprocess):
  python -m tool.src.fake_target.fake_target --port /dev/pts/3

Usage (in-process):
  from tool.src.fake_target.fake_target import FakeTargetInProcess
  target = FakeTargetInProcess()
  target.start()
  # Use target.client_port for BmsProtocolClient
"""
import struct
import threading
import io
from typing import Optional

from ..protocol.framing import encode_frame, FrameDecoder, FrameError
from ..protocol.packet_defs import (
    PKT_GET_CAPABILITIES, PKT_GET_VALUES, PKT_GET_CELLS, PKT_GET_TEMPS,
    PKT_GET_FAULTS, PKT_CLEAR_LATCHED_FAULTS,
    PKT_GET_CONFIG, PKT_VALIDATE_CONFIG, PKT_SET_CONFIG_RAM, PKT_STORE_CONFIG,
    PKT_GET_BOOT_INFO, PKT_ENTER_BOOTLOADER,
    PKT_BOOT_UPDATE_BEGIN, PKT_BOOT_UPDATE_CHUNK,
    PKT_BOOT_UPDATE_FINALIZE, PKT_BOOT_UPDATE_ABORT,
    PROTOCOL_VERSION, HW_PROFILE_ID, CONFIG_SCHEMA_SIZE,
    FIRMWARE_TYPE_BMS_APP, TOTAL_CELL_COUNT, TOTAL_TEMP_COUNT,
)
from ..config.schema import BmsConfig


TEMP_INVALID = 0x8000  # as int16, represents INVALID


class FakeTarget:
    """Core fake target state machine. Feed bytes in, call process() to get response bytes."""

    def __init__(self):
        self._decoder = FrameDecoder()
        self._config = BmsConfig()
        self._active_faults  = 0
        self._latched_faults = 0
        self._seq_out = 0
        # Default cell voltages: 3700 mV each
        self._cell_mv = [3700] * TOTAL_CELL_COUNT
        # Default temps: 250 (25.0°C)
        self._temps_cx10 = [250] * TOTAL_TEMP_COUNT
        self._uptime_ms = 0

    def feed(self, data: bytes) -> bytes:
        """Feed incoming bytes; returns all response bytes."""
        frames = self._decoder.feed(data)
        out = b''
        for f in frames:
            out += self._handle(f)
        return out

    def _respond(self, pkt_id: int, payload: bytes, seq: int,
                 is_error: bool = False) -> bytes:
        return encode_frame(pkt_id, payload, seq=seq, is_response=True, is_error=is_error)

    def _error(self, pkt_id: int, seq: int, code: int) -> bytes:
        return self._respond(pkt_id, bytes([code]), seq, is_error=True)

    def _handle(self, frame: dict) -> bytes:
        pkt_id  = frame['pkt_id']
        seq     = frame['seq']
        payload = frame['payload']

        if pkt_id == PKT_GET_CAPABILITIES:
            return self._h_capabilities(seq)
        elif pkt_id == PKT_GET_VALUES:
            return self._h_values(seq)
        elif pkt_id == PKT_GET_CELLS:
            return self._h_cells(seq, payload)
        elif pkt_id == PKT_GET_TEMPS:
            return self._h_temps(seq)
        elif pkt_id == PKT_GET_FAULTS:
            return self._h_faults(seq)
        elif pkt_id == PKT_CLEAR_LATCHED_FAULTS:
            return self._h_clear_latched(seq, payload)
        elif pkt_id == PKT_GET_CONFIG:
            return self._h_get_config(seq)
        elif pkt_id == PKT_VALIDATE_CONFIG:
            return self._h_validate_config(seq, payload)
        elif pkt_id == PKT_SET_CONFIG_RAM:
            return self._h_set_config_ram(seq, payload)
        elif pkt_id == PKT_STORE_CONFIG:
            return self._error(pkt_id, seq, 0x0A)  # ERR_NOT_SUPPORTED
        elif pkt_id == PKT_GET_BOOT_INFO:
            return self._h_boot_info(seq)
        elif pkt_id == PKT_ENTER_BOOTLOADER:
            return self._h_enter_bootloader(seq, payload)
        else:
            return self._error(pkt_id, seq, 0x01)  # ERR_UNKNOWN_PACKET

    def _h_capabilities(self, seq: int) -> bytes:
        flags = 0x1F  # all features
        # Build manually to be byte-exact (26 bytes)
        resp = bytearray(26)
        struct.pack_into('<H', resp, 0, FIRMWARE_TYPE_BMS_APP)
        resp[2] = 0; resp[3] = 1; resp[4] = 0  # version 0.1.0
        struct.pack_into('<H', resp, 5, HW_PROFILE_ID)
        struct.pack_into('<H', resp, 7, PROTOCOL_VERSION)
        struct.pack_into('<H', resp, 9, 1)       # config schema
        resp[11] = TOTAL_CELL_COUNT
        resp[12] = TOTAL_TEMP_COUNT
        struct.pack_into('<I', resp, 13, flags)
        resp[17] = 9
        struct.pack_into('<I', resp, 18, 188*1024)
        struct.pack_into('<I', resp, 22, 8*1024)
        return self._respond(PKT_GET_CAPABILITIES, bytes(resp), seq)

    def _h_values(self, seq: int) -> bytes:
        resp = bytearray(36)
        struct.pack_into('<i', resp, 0, 0)         # vbat_mv (invalid)
        struct.pack_into('<i', resp, 4, 0)         # vpack_mv
        struct.pack_into('<i', resp, 8, 0)         # i_batt_ma
        struct.pack_into('<H', resp, 12, 1)        # state: STANDBY
        struct.pack_into('<Q', resp, 14, self._active_faults)
        struct.pack_into('<Q', resp, 22, self._latched_faults)
        resp[30] = 0                               # outputs_state
        struct.pack_into('<I', resp, 31, self._uptime_ms)
        resp[35] = 0                               # measurement_flags (all invalid)
        return self._respond(PKT_GET_VALUES, bytes(resp), seq)

    def _h_cells(self, seq: int, payload: bytes) -> bytes:
        include_validity = bool(payload and payload[0] & 1)
        resp = bytearray(156 + (10 if include_validity else 0))
        struct.pack_into('<H', resp, 0, TOTAL_CELL_COUNT)
        for i, mv in enumerate(self._cell_mv):
            struct.pack_into('<H', resp, 2 + i*2, mv)
        struct.pack_into('<I', resp, 152, self._uptime_ms)
        if include_validity:
            # All cells valid
            for i in range(TOTAL_CELL_COUNT):
                resp[156 + i//8] |= (1 << (i % 8))
        return self._respond(PKT_GET_CELLS, bytes(resp), seq)

    def _h_temps(self, seq: int) -> bytes:
        resp = bytearray(2 + TOTAL_TEMP_COUNT * 2)
        struct.pack_into('<H', resp, 0, TOTAL_TEMP_COUNT)
        for i, t in enumerate(self._temps_cx10):
            struct.pack_into('<h', resp, 2 + i*2, t)
        return self._respond(PKT_GET_TEMPS, bytes(resp), seq)

    def _h_faults(self, seq: int) -> bytes:
        resp = struct.pack('<QQ', self._active_faults, self._latched_faults)
        return self._respond(PKT_GET_FAULTS, resp, seq)

    def _h_clear_latched(self, seq: int, payload: bytes) -> bytes:
        if len(payload) < 8:
            return self._error(PKT_CLEAR_LATCHED_FAULTS, seq, 0x02)
        mask = struct.unpack_from('<Q', payload)[0]
        clearable = mask & self._latched_faults & ~self._active_faults
        self._latched_faults &= ~clearable
        return self._respond(PKT_CLEAR_LATCHED_FAULTS, struct.pack('<Q', clearable), seq)

    def _h_get_config(self, seq: int) -> bytes:
        return self._respond(PKT_GET_CONFIG, self._config.pack(), seq)

    def _h_validate_config(self, seq: int, payload: bytes) -> bytes:
        from ..config.validator import validate_config
        if len(payload) != CONFIG_SCHEMA_SIZE:
            return self._error(PKT_VALIDATE_CONFIG, seq, 0x02)
        cfg = BmsConfig.unpack(payload)
        ok, err_off, _ = validate_config(cfg)
        resp = struct.pack('<BH', 0 if ok else 1, err_off)
        return self._respond(PKT_VALIDATE_CONFIG, resp, seq)

    def _h_set_config_ram(self, seq: int, payload: bytes) -> bytes:
        from ..config.validator import validate_config
        if len(payload) != CONFIG_SCHEMA_SIZE:
            return self._error(PKT_SET_CONFIG_RAM, seq, 0x02)
        cfg = BmsConfig.unpack(payload)
        ok, err_off, _ = validate_config(cfg)
        if ok:
            self._config = cfg
        resp = struct.pack('<BH', 0 if ok else 1, err_off)
        return self._respond(PKT_SET_CONFIG_RAM, resp, seq)

    def _h_boot_info(self, seq: int) -> bytes:
        # Minimal stub response
        resp = bytes(20)
        return self._respond(PKT_GET_BOOT_INFO, resp, seq)

    def _h_enter_bootloader(self, seq: int, payload: bytes) -> bytes:
        if len(payload) < 4:
            return self._error(PKT_ENTER_BOOTLOADER, seq, 0x02)
        magic = struct.unpack_from('<I', payload)[0]
        if magic != 0xB007B007:
            return self._error(PKT_ENTER_BOOTLOADER, seq, 0x0B)
        return self._respond(PKT_ENTER_BOOTLOADER, b'', seq)

    # ── State injection helpers (for test scenarios) ──────────────────────────

    def inject_fault(self, fault_bit: int) -> None:
        self._active_faults  |= (1 << fault_bit)
        self._latched_faults |= (1 << fault_bit)

    def clear_fault(self, fault_bit: int) -> None:
        self._active_faults &= ~(1 << fault_bit)

    def set_cell_mv(self, values: list) -> None:
        self._cell_mv = list(values)[:TOTAL_CELL_COUNT]

    def set_temps_cx10(self, values: list) -> None:
        self._temps_cx10 = list(values)[:TOTAL_TEMP_COUNT]


# ── Subprocess entry point ────────────────────────────────────────────────────

def run_on_serial_port(port_name: str, baud: int = 115200) -> None:
    """Run the fake target on a real or virtual serial port."""
    import serial
    target = FakeTarget()
    with serial.Serial(port_name, baud, timeout=0.1) as ser:
        print(f"[fake_target] listening on {port_name} at {baud}")
        while True:
            data = ser.read(512)
            if data:
                resp = target.feed(data)
                if resp:
                    ser.write(resp)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='BMS fake target')
    parser.add_argument('--port', required=True, help='Serial port (e.g. /dev/pts/3)')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()
    run_on_serial_port(args.port, args.baud)
