"""test_bringup_cli.py — Tests for bring-up diagnostic CLI commands.

All tests run against FakeTargetInProcess (in-process, no TCP).
Covers: diag gpio, diag outputs, probe cell-chain, probe temp-chain,
        probe isl, read vpack-raw, balance disable-all.
"""
import struct
import pytest

from tool.src.fake_target.fake_target import FakeTargetInProcess
from tool.src.protocol.client import BmsProtocolClient, ErrorResponse, ProtocolError
from tool.src.core.target_model import TargetModel, TargetRefusedError
from tool.src.cli.bmsctl import (
    cmd_diag_gpio, cmd_diag_outputs,
    cmd_probe_cell_chain, cmd_probe_temp_chain,
    cmd_probe_isl, cmd_read_vpack_raw, cmd_balance_disable_all,
    main,
)


# ── helpers ───────────────────────────────────────────────────────────────────

class _FakePort:
    """Synchronous in-process port adaptor for BmsProtocolClient."""
    def __init__(self, mode: str = 'healthy'):
        self._ft = FakeTargetInProcess(mode=mode)
        self._buf = b''

    def write(self, data: bytes) -> None:
        self._buf += self._ft.exchange(data)

    def read(self, n: int) -> bytes:
        chunk, self._buf = self._buf[:n], self._buf[n:]
        return chunk

    @property
    def in_waiting(self) -> int:
        return len(self._buf)


def _client(mode: str = 'healthy') -> BmsProtocolClient:
    return BmsProtocolClient(_FakePort(mode))


def _model(mode: str = 'healthy') -> TargetModel:
    m = TargetModel(_FakePort(mode))
    m.capabilities_handshake()
    return m


class _Args:
    """Minimal argparse namespace for CLI command tests."""
    host   = '127.0.0.1'
    port   = 65102
    serial = None
    baud   = 115200
    json   = False

    def __init__(self, mode: str = 'healthy', **kw):
        self._mode = mode
        for k, v in kw.items():
            setattr(self, k, v)


# We monkey-patch _connect inside each test so commands use the in-process fake.
def _make_connect(mode: str):
    from tool.src.core.connection_manager import ConnectionManager
    class _FakeMgr:
        def disconnect(self): pass
    def _connect_patch(args):
        m = TargetModel(_FakePort(mode))
        m.capabilities_handshake()
        return _FakeMgr(), m
    return _connect_patch


import tool.src.cli.bmsctl as _bmsctl_mod


# ── diag gpio ─────────────────────────────────────────────────────────────────

class TestDiagGpio:
    def test_returns_9_fields(self):
        r = _model().get_gpio_snapshot()
        assert set(r.keys()) >= {
            'cs_cell', 'cs_temp', 'power_button', 'charge_detect',
            'power_enable', 'master_ok_raw', 'discharge_raw',
            'charge_raw', 'charger_safety_raw',
        }

    def test_healthy_cs_idle_high(self):
        r = _model().get_gpio_snapshot()
        assert r['cs_cell'] == 1
        assert r['cs_temp'] == 1

    def test_healthy_outputs_inactive(self):
        r = _model().get_gpio_snapshot()
        assert r['master_ok_raw']      == 0
        assert r['discharge_raw']      == 0
        assert r['charge_raw']         == 0
        assert r['charger_safety_raw'] == 0

    def test_bootloader_mode_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').get_gpio_snapshot()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_diag_gpio(_Args()) == 0


# ── diag outputs ──────────────────────────────────────────────────────────────

class TestDiagOutputs:
    def test_healthy_all_inactive(self):
        r = _model().get_outputs_snapshot()
        assert r['logical_state'] == 0
        assert r['raw_state']     == 0

    def test_has_both_fields(self):
        r = _model().get_outputs_snapshot()
        assert 'logical_state' in r
        assert 'raw_state'     in r

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').get_outputs_snapshot()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_diag_outputs(_Args()) == 0


# ── probe cell-chain ──────────────────────────────────────────────────────────

class TestProbeCellChain:
    def test_healthy_status_ok(self):
        r = _model().probe_cell_chain()
        assert r['status']   == 0
        assert r['ic_count'] == 5

    def test_healthy_all_ics_responded(self):
        r = _model().probe_cell_chain()
        assert len(r['ics']) == 5
        for ic in r['ics']:
            assert ic['responded'] is True
            assert len(ic['cfga']) == 12   # 6 bytes as hex = 12 chars

    def test_healthy_cfga0_nominal(self):
        r = _model().probe_cell_chain()
        for ic in r['ics']:
            assert ic['cfga'][:2] == 'f8'  # CFGA[0] = 0xF8

    def test_isospi_fault_status_fail(self):
        r = _model('isospi_fault').probe_cell_chain()
        assert r['status'] != 0

    def test_isospi_fault_no_ic_response(self):
        r = _model('isospi_fault').probe_cell_chain()
        for ic in r['ics']:
            assert ic['responded'] is False

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').probe_cell_chain()

    def test_cli_exit_0_healthy(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_probe_cell_chain(_Args()) == 0

    def test_cli_exit_1_isospi_fault(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('isospi_fault'))
        assert cmd_probe_cell_chain(_Args()) == 1


# ── probe temp-chain ──────────────────────────────────────────────────────────

class TestProbeTempChain:
    def test_healthy_status_ok(self):
        r = _model().probe_temp_chain()
        assert r['status']   == 0
        assert r['ic_count'] == 5

    def test_healthy_all_responded(self):
        r = _model().probe_temp_chain()
        for ic in r['ics']:
            assert ic['responded'] is True

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').probe_temp_chain()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_probe_temp_chain(_Args()) == 0


# ── probe isl28022 ────────────────────────────────────────────────────────────

class TestProbeIsl28022:
    def test_healthy_status_ok(self):
        r = _model().probe_isl28022()
        assert r['status'] == 0

    def test_healthy_config_reg_nonzero(self):
        r = _model().probe_isl28022()
        # Nominal fake value is 0x4127
        assert r['config_reg'] == 0x4127

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').probe_isl28022()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_probe_isl(_Args()) == 0


# ── read vpack-raw ────────────────────────────────────────────────────────────

class TestReadVpackRaw:
    def test_healthy_status_ok(self):
        r = _model().read_vpack_raw()
        assert r['status'] == 0

    def test_healthy_raw_code_in_range(self):
        r = _model().read_vpack_raw()
        assert 0 <= r['raw_code'] <= 4095

    def test_vpack_invalid_fault_status_fail(self):
        r = _model('vpack_invalid').read_vpack_raw()
        assert r['status'] != 0

    def test_vpack_invalid_raw_zero(self):
        r = _model('vpack_invalid').read_vpack_raw()
        assert r['raw_code'] == 0

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').read_vpack_raw()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_read_vpack_raw(_Args()) == 0

    def test_cli_exit_1_fault(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('vpack_invalid'))
        assert cmd_read_vpack_raw(_Args()) == 1


# ── balance disable-all ───────────────────────────────────────────────────────

class TestBalanceDisableAll:
    def test_healthy_returns_true(self):
        assert _model().balance_disable_all() is True

    def test_bootloader_returns_error(self):
        with pytest.raises(TargetRefusedError):
            _model('bootloader').balance_disable_all()

    def test_cli_exit_0(self, monkeypatch):
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        assert cmd_balance_disable_all(_Args()) == 0

    def test_can_be_called_multiple_times(self):
        m = _model()
        assert m.balance_disable_all() is True
        assert m.balance_disable_all() is True


# ── target_model safety gates ─────────────────────────────────────────────────

class TestBringupSafetyGates:
    def test_requires_app_mode_gpio(self):
        m = TargetModel(_FakePort('bootloader'))
        m.capabilities_handshake()
        with pytest.raises(TargetRefusedError):
            m.get_gpio_snapshot()

    def test_requires_app_mode_probe_cell(self):
        m = TargetModel(_FakePort('bootloader'))
        m.capabilities_handshake()
        with pytest.raises(TargetRefusedError):
            m.probe_cell_chain()

    def test_requires_app_mode_balance(self):
        m = TargetModel(_FakePort('bootloader'))
        m.capabilities_handshake()
        with pytest.raises(TargetRefusedError):
            m.balance_disable_all()


# ── JSON output schema ────────────────────────────────────────────────────────

class TestJsonOutputSchema:
    def test_gpio_json_has_all_keys(self, monkeypatch, capsys):
        import json
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        args = _Args(json=True)
        cmd_diag_gpio(args)
        data = json.loads(capsys.readouterr().out)
        for key in ('cs_cell', 'cs_temp', 'power_button', 'power_enable',
                    'master_ok_raw', 'discharge_raw', 'charge_raw', 'charger_safety_raw'):
            assert key in data

    def test_probe_cell_json_has_ics(self, monkeypatch, capsys):
        import json
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        args = _Args(json=True)
        cmd_probe_cell_chain(args)
        data = json.loads(capsys.readouterr().out)
        assert 'ics' in data
        assert len(data['ics']) == 5

    def test_vpack_json_has_raw_code(self, monkeypatch, capsys):
        import json
        monkeypatch.setattr(_bmsctl_mod, '_connect', _make_connect('healthy'))
        args = _Args(json=True)
        cmd_read_vpack_raw(args)
        data = json.loads(capsys.readouterr().out)
        assert 'raw_code' in data
        assert 'status'   in data
