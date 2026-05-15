#!/usr/bin/env python3
"""generate_protocol.py — regenerate C header and Python constants from YAML sources.

Sources:
  protocol/packet_ids.yaml    → firmware/include/bms_protocol_ids.h
                              → tool/src/protocol/packet_defs.py

Run from repo root:
  python3 scripts/generate_protocol.py
"""
import sys
import os
import yaml
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent

def load_yaml(path: Path) -> dict:
    with open(path) as f:
        return yaml.safe_load(f)

def generate_c_header(packets: list, meta: dict) -> str:
    lines = [
        "/* bms_protocol_ids.h — packet ID constants.",
        f" * Generated from: protocol/packet_ids.yaml",
        " * Do not edit manually — regenerate with: scripts/generate_protocol.py",
        " */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "/* ── Packet IDs ─────────────────────────────────────────── */",
    ]
    for pkt in packets:
        name = pkt['name']
        pid  = pkt['id']
        lines.append(f"#define PKT_{name:<30s} ((uint16_t){pid:#06x}u)")
    lines += [
        "",
        f"#define PROTOCOL_VERSION  ((uint16_t){meta['protocol_version']}u)",
    ]
    return '\n'.join(lines) + '\n'

def generate_python_defs(packets: list, meta: dict) -> str:
    lines = [
        '"""packet_defs.py — packet ID constants.',
        'Generated from: protocol/packet_ids.yaml',
        'Do not edit manually — regenerate with: scripts/generate_protocol.py',
        '"""',
        '',
    ]
    for pkt in packets:
        name = pkt['name']
        pid  = pkt['id']
        lines.append(f"PKT_{name:<30s} = {pid:#06x}")
    lines += [
        '',
        f"PROTOCOL_VERSION    = {meta['protocol_version']}",
        "HW_PROFILE_ID       = 0x0001",
        "CONFIG_SCHEMA_SIZE  = 226",
        "TOTAL_CELL_COUNT    = 75",
        "TOTAL_TEMP_COUNT    = 75",
        "",
        "FIRMWARE_TYPE_BMS_APP    = 0x0001",
        "FIRMWARE_TYPE_BOOTLOADER = 0x0002",
        "",
        "ENTER_BOOTLOADER_MAGIC = 0xB007B007",
    ]
    return '\n'.join(lines) + '\n'

def main():
    pkt_yaml_path = REPO_ROOT / 'protocol' / 'packet_ids.yaml'
    if not pkt_yaml_path.exists():
        print(f"Error: {pkt_yaml_path} not found", file=sys.stderr)
        sys.exit(1)

    data    = load_yaml(pkt_yaml_path)
    packets = data['packets']
    meta    = data['meta']

    # C header
    c_header = generate_c_header(packets, meta)
    c_out = REPO_ROOT / 'firmware' / 'include' / 'bms_protocol_ids.h'
    c_out.write_text(c_header)
    print(f"  Written: {c_out}")

    # Python
    py_defs = generate_python_defs(packets, meta)
    py_out = REPO_ROOT / 'tool' / 'src' / 'protocol' / 'packet_defs.py'
    py_out.write_text(py_defs)
    print(f"  Written: {py_out}")

    print("Done.")

if __name__ == '__main__':
    main()
