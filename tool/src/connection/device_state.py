"""device_state.py — connection state and device mode tracking."""
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import Optional


class DeviceMode(Enum):
    DISCONNECTED = auto()
    CONNECTING   = auto()
    BMS_APP      = auto()   # firmware_type == 0x0001
    BOOTLOADER   = auto()   # firmware_type == 0x0002
    UNKNOWN      = auto()   # connected but unrecognised firmware_type
    UNSUPPORTED  = auto()   # protocol_version mismatch or hw_profile mismatch


@dataclass
class CapabilitiesState:
    firmware_type:         int  = 0
    firmware_version:      tuple = (0, 0, 0)
    hw_profile_id:         int  = 0
    protocol_version:      int  = 0
    config_schema_version: int  = 0
    cell_count:            int  = 0
    temp_count:            int  = 0
    feature_flags:         int  = 0


@dataclass
class DeviceState:
    mode:         DeviceMode       = DeviceMode.DISCONNECTED
    capabilities: Optional[CapabilitiesState] = None
    port_name:    str              = ''
    error_msg:    str              = ''

    def is_connected(self) -> bool:
        return self.mode not in (DeviceMode.DISCONNECTED, DeviceMode.CONNECTING)

    def is_bms_app(self) -> bool:
        return self.mode == DeviceMode.BMS_APP

    def is_bootloader(self) -> bool:
        return self.mode == DeviceMode.BOOTLOADER

    def can_config(self) -> bool:
        return self.mode == DeviceMode.BMS_APP

    def can_update(self) -> bool:
        return self.mode in (DeviceMode.BMS_APP, DeviceMode.BOOTLOADER)
